#include "services/TransactionService.hpp"
#include "core/AuthSecurity.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <string>

namespace Banking {
namespace {

int64_t readFeeConfig(TursoDB& database, const std::string& key, int64_t fallback) {
    const json response = database.execute(
        "SELECT value FROM system_config WHERE key = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", key}}}));
    try {
        const auto cell = response["results"][0]["response"]["result"]["rows"][0][0];
        if (!cell.is_object() || !cell.contains("value") || cell["value"].is_null()) return fallback;
        const auto& value = cell["value"];
        const std::string text = value.is_string() ? value.get<std::string>() : value.dump();
        const int64_t parsed = std::stoll(text);
        return parsed >= 0 && parsed <= 1000000000LL ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

bool logCounterExecution(AuditRepository& auditRepo, const std::string& executionId,
                         const std::string& operatorId, const std::string& action,
                         const std::string& targetAccountId, int64_t amountVnd,
                         const std::string& transactionId, const std::string& result,
                         const std::string& reason) {
    std::string details = "execution_id=" + executionId + "; amount_vnd=" + std::to_string(amountVnd) +
                          "; result=" + result + "; transaction_id=" +
                          (transactionId.empty() ? "-" : transactionId);
    if (!reason.empty()) details += "; reason=" + reason;
    return auditRepo.logActionWithId(executionId, operatorId, action,
                                     targetAccountId.empty() ? "UNKNOWN" : targetAccountId, details);
}

std::string counterDetailValue(const std::string& details, const std::string& key) {
    const std::string marker = key + "=";
    const auto start = details.find(marker);
    if (start == std::string::npos) return {};
    const auto valueStart = start + marker.size();
    const auto end = details.find(';', valueStart);
    return details.substr(valueStart, end == std::string::npos ? std::string::npos : end - valueStart);
}

std::optional<TransactionResult> recoveredTransaction(TursoDB& database, const std::string& pendingValue,
                                                      const std::string& successMessage) {
    if (pendingValue.rfind("PENDING:", 0) != 0) return std::nullopt;
    const std::string transactionId = pendingValue.substr(8);
    const json transaction = database.execute(
        "SELECT post_balance_vnd FROM transactions WHERE transaction_id = ? AND status = 'SUCCESS' LIMIT 1;",
        json::array({{{"type", "text"}, {"value", transactionId}}}));
    try {
        const auto& rows = transaction["results"][0]["response"]["result"]["rows"];
        if (rows.is_array() && !rows.empty()) {
            const auto& cell = rows[0][0];
            const std::string balance = cell.is_object() && cell.contains("value") && !cell["value"].is_null()
                ? (cell["value"].is_string() ? cell["value"].get<std::string>() : cell["value"].dump()) : "0";
            return TransactionResult{true, successMessage, transactionId, std::stoll(balance)};
        }
    } catch (...) {}
    return std::nullopt;
}

std::optional<CounterOperationResult> previousCounterExecution(
    TransactionRepository& txnRepo, AuditRepository& auditRepo, TursoDB& database,
    const std::string& executionId, const std::string& operatorId, const std::string& idempotencyAction,
    const std::string& successAction, const std::string& failureAction, const std::string& successMessage) {
    if (executionId.empty()) return std::nullopt;
    if (const auto cached = txnRepo.getIdempotentResponse(executionId, operatorId, idempotencyAction)) {
        if (cached->rfind("PENDING:", 0) == 0) {
            const std::string transactionId = cached->substr(8);
            const json transaction = database.execute(
                "SELECT post_balance_vnd FROM transactions WHERE transaction_id = ? AND status = 'SUCCESS' LIMIT 1;",
                json::array({{{"type", "text"}, {"value", transactionId}}}));
            try {
                const auto& rows = transaction["results"][0]["response"]["result"]["rows"];
                if (rows.is_array() && !rows.empty()) {
                    const auto& cell = rows[0][0];
                    const std::string balance = cell.is_object() && cell.contains("value") && !cell["value"].is_null()
                        ? (cell["value"].is_string() ? cell["value"].get<std::string>() : cell["value"].dump()) : "0";
                    return CounterOperationResult{true, successMessage, transactionId, std::stoll(balance), executionId, true};
                }
            } catch (...) {}
            return CounterOperationResult{false, "Mã này đang được xử lý; hãy tra cứu lại bằng chính mã đó.",
                                          "", 0, executionId, false};
        }
        try {
            auto result = json::parse(*cached).get<CounterOperationResult>();
            result.trace_persisted = true;
            return result;
        } catch (...) {
            return CounterOperationResult{false, "Không đọc được kết quả đã lưu cho mã thực thi này.",
                                          "", 0, executionId, true};
        }
    }

    const auto previous = auditRepo.getLogById(executionId);
    if (!previous) return std::nullopt;
    if (previous->operator_id != operatorId ||
        (previous->action != successAction && previous->action != failureAction)) {
        return CounterOperationResult{false, "Mã thực thi đã được dùng cho một yêu cầu khác.",
                                      "", 0, executionId, false};
    }
    if (previous->action == failureAction) {
        std::string reason = counterDetailValue(previous->details, "reason");
        if (reason.empty()) reason = "Yêu cầu này đã thất bại trước đó; hãy tạo mã mới nếu muốn thử lại.";
        return CounterOperationResult{false, reason, "", 0, executionId, true};
    }

    const std::string transactionId = counterDetailValue(previous->details, "transaction_id");
    if (transactionId.empty() || transactionId == "-") {
        return CounterOperationResult{false, "Không khôi phục được giao dịch đã ghi nhận; không gửi lại bằng mã mới.",
                                      "", 0, executionId, true};
    }
    const json transaction = database.execute(
        "SELECT post_balance_vnd FROM transactions WHERE transaction_id = ? AND status = 'SUCCESS' LIMIT 1;",
        json::array({{{"type", "text"}, {"value", transactionId}}}));
    try {
        const auto& rows = transaction["results"][0]["response"]["result"]["rows"];
        if (!rows.is_array() || rows.empty()) throw std::runtime_error("missing transaction");
        const auto& cell = rows[0][0];
        const std::string balance = cell.is_object() && cell.contains("value") && !cell["value"].is_null()
            ? (cell["value"].is_string() ? cell["value"].get<std::string>() : cell["value"].dump()) : "0";
        return CounterOperationResult{true, successMessage, transactionId, std::stoll(balance), executionId, true};
    } catch (...) {
        return CounterOperationResult{false, "Không thể đọc kết quả giao dịch đã ghi nhận.", "", 0, executionId, true};
    }
}

} // namespace

TransactionService::TransactionService(TransactionRepository& txnRepo, UserRepository& userRepo,
                                       AccountRepository& accRepo, BillRepository& billRepo,
                                       AuditRepository& auditRepo, SessionManager& sessionManager, TursoDB& tursoDb)
    : m_txnRepo(txnRepo), m_userRepo(userRepo), m_accRepo(accRepo),
      m_billRepo(billRepo), m_auditRepo(auditRepo), m_sessionManager(sessionManager), m_tursoDb(tursoDb) {}

std::string TransactionService::getCurrentDateStr() {
    std::time_t now = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::gmtime(&now));
    return std::string(buf);
}

OtpChallengeResult TransactionService::requestOtp(const OtpChallengeRequest& request) {
    OtpChallengeResult result;
    const auto session = m_sessionManager.getSession(request.session_token);
    if (!session || session->role != "CUSTOMER") { result.message = "Phiên khách hàng không hợp lệ."; return result; }
    if (request.amount_vnd <= OTP_THRESHOLD_VND || request.amount_vnd > SINGLE_TXN_LIMIT_VND ||
        request.idempotency_key.empty() || request.receiver_account_id.empty()) {
        result.message = "Thông tin giao dịch yêu cầu OTP không hợp lệ.";
        return result;
    }
    const std::string purpose = request.purpose.empty() ? "TRANSFER" : request.purpose;
    if ((purpose != "TRANSFER" && purpose != "WITHDRAW") ||
        (purpose == "WITHDRAW" && request.receiver_account_id != "CASH_WITHDRAW") ||
        (purpose == "TRANSFER" && request.receiver_account_id == "CASH_WITHDRAW")) {
        result.message = "Mục đích OTP không hợp lệ.";
        return result;
    }
    const std::string challengeId = "OTP_" + AuthSecurity::generateNumericCode(24);
    const std::string code = AuthSecurity::generateOTP(6);
    const std::string codeHash = AuthSecurity::hashPin(challengeId + ":" + code);
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string notificationId = "NTF_OTP_" + challengeId;
    const json inserted = m_tursoDb.executeBatch({
        {"INSERT INTO otp_challenges (challenge_id, code_hash, user_id, context_reference, amount_vnd, recipient_account_id, expires_at) VALUES (?, ?, ?, ?, ?, ?, ?);",
         json::array({{{"type", "text"}, {"value", challengeId}}, {{"type", "text"}, {"value", codeHash}},
                      {{"type", "text"}, {"value", session->user_id}}, {{"type", "text"}, {"value", request.idempotency_key}},
                      {{"type", "numeric"}, {"value", request.amount_vnd}}, {{"type", "text"}, {"value", request.receiver_account_id}},
                      {{"type", "numeric"}, {"value", now + 180}}})},
        {"INSERT INTO notifications (notification_id, user_id, title, body, notification_type, reference_id) "
         "SELECT ?, ?, 'Mã OTP xác thực giao dịch', 'Mã OTP đã được tạo trong ứng dụng và có hiệu lực trong 3 phút. Mã chỉ hiển thị tại màn hình xác thực.', 'OTP', ? "
         "WHERE EXISTS (SELECT 1 FROM otp_challenges WHERE challenge_id = ? AND user_id = ?);",
         json::array({{{"type", "text"}, {"value", notificationId}}, {{"type", "text"}, {"value", session->user_id}},
                      {{"type", "text"}, {"value", challengeId}}, {{"type", "text"}, {"value", challengeId}},
                      {{"type", "text"}, {"value", session->user_id}}})}
    });
    if (!inserted.contains("results") || !inserted["results"].is_array() || inserted["results"].empty() ||
        !std::all_of(inserted["results"].begin(), inserted["results"].end(), [](const json& item) {
            return item.is_object() && item.value("type", "") == "ok";
        })) {
        result.message = "Không lưu được phiên OTP vào database. Hãy yêu cầu mã mới.";
        return result;
    }
    result.success = true;
    result.challenge_id = challengeId;
    result.otp_code = code;
    result.message = "Đã tạo mã OTP trong ứng dụng. Mã có hiệu lực trong 3 phút.";
    return result;
}

bool TransactionService::verifyOtp(const std::string& userId, const std::string& challengeId,
                                   const std::string& otp, const std::string& reference,
                                   int64_t amount, const std::string& recipient, std::string& error) const {
    if (challengeId.empty() || otp.size() != 6 || reference.empty() ||
        !std::all_of(otp.begin(), otp.end(), [](unsigned char c) { return c >= '0' && c <= '9'; })) {
        error = "Thiếu mã OTP hoặc phiên xác thực.";
        return false;
    }
    const json challenge = m_tursoDb.execute(
        "SELECT code_hash, context_reference, amount_vnd, recipient_account_id, expires_at, used_at "
        "FROM otp_challenges WHERE challenge_id = ? AND user_id = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", challengeId}}, {{"type", "text"}, {"value", userId}}}));
    try {
        const auto row = challenge["results"][0]["response"]["result"]["rows"][0];
        auto cell = [](const json& cellValue) {
            if (!cellValue.is_object() || !cellValue.contains("value") || cellValue["value"].is_null()) return std::string{};
            const auto& value = cellValue["value"];
            return value.is_string() ? value.get<std::string>() : value.dump();
        };
        if (cell(row[1]) != reference || std::stoll(cell(row[2])) != amount || cell(row[3]) != recipient ||
            !cell(row[5]).empty() || std::stoll(cell(row[4])) <= std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()) {
            error = "OTP hết hạn, đã sử dụng hoặc không khớp giao dịch này.";
            return false;
        }
        const std::string expectedHash = cell(row[0]);
        if (expectedHash.empty() || AuthSecurity::hashPin(challengeId + ":" + otp) != expectedHash) {
            error = "Mã OTP không chính xác.";
            return false;
        }
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const json claimed = m_tursoDb.execute(
            "UPDATE otp_challenges SET used_at = ? WHERE challenge_id = ? AND user_id = ? AND used_at IS NULL AND expires_at > ? RETURNING challenge_id;",
            json::array({{{"type", "numeric"}, {"value", now}}, {{"type", "text"}, {"value", challengeId}},
                         {{"type", "text"}, {"value", userId}}, {{"type", "numeric"}, {"value", now}}}));
        const auto rows = claimed["results"][0]["response"]["result"]["rows"];
        if (!rows.is_array() || rows.empty()) { error = "OTP đã được sử dụng hoặc hết hạn."; return false; }

        return true;
    } catch (...) {
        error = "Không xác minh được OTP với database.";
        return false;
    }
}

// =========================================================================
// 1. LUỒNG CHUYỂN TIỀN (TransferTransaction)
// =========================================================================
TransactionResult TransactionService::executeTransfer(const TransferRequest& req) {
    TransactionResult res;

    // Xác thực phiên trước khi tra cứu khóa idempotency để không làm lộ kết quả giao dịch.
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên đăng nhập không hợp lệ hoặc đã hết hạn!";
        return res;
    }
    const auto& sess = sessOpt.value();
    if (req.idempotency_key.empty()) {
        res.message = "Thiếu mã chống gửi trùng cho yêu cầu chuyển tiền.";
        return res;
    }
    {
        auto cached = m_txnRepo.getIdempotentResponse(req.idempotency_key, sess.user_id, "TRANSFER");
        if (cached && !cached->empty()) {
            if (cached->rfind("PENDING:", 0) == 0) {
                if (const auto recovered = recoveredTransaction(m_tursoDb, *cached, "Chuyển tiền thành công.")) return *recovered;
                res.message = "Yêu cầu chuyển tiền đang được xử lý; hãy gửi lại bằng chính mã yêu cầu này.";
                return res;
            }
            try { return json::parse(*cached).get<TransactionResult>(); }
            catch (...) { res.message = "Không đọc được kết quả chuyển tiền đã lưu."; return res; }
        }
    }

    // 3. Chặn chuyển tiền cho chính tài khoản nguồn
    const std::string receiverId = req.is_interbank ? ("EXT:" + req.receiver_bank + ":" + req.receiver_account_id) : req.receiver_account_id;
    const std::vector<std::string> simulatedBanks{"MB", "AGRIBANK", "VIETCOMBANK", "BIDV", "TECHCOMBANK", "ACB"};
    if (req.is_interbank && (std::find(simulatedBanks.begin(), simulatedBanks.end(), req.receiver_bank) == simulatedBanks.end() || req.receiver_account_id.size() < 8 || req.receiver_account_id.size() > 16 ||
        !std::all_of(req.receiver_account_id.begin(), req.receiver_account_id.end(), [](unsigned char c){ return std::isdigit(c); }))) {
        res.message = "Ngân hàng hoặc số tài khoản thụ hưởng không hợp lệ."; return res;
    }
    if (sess.primary_account_id == receiverId || (!req.is_interbank && sess.primary_account_id == req.receiver_account_id)) {
        res.message = "Tài khoản thụ hưởng không được trùng với tài khoản chuyển!";
        return res;
    }

    // 4. XÁC THỰC MÃ PIN GIAO DỊCH (6 số)
    auto userOpt = m_userRepo.findById(sess.user_id);
    if (!userOpt || !userOpt->verifyPin(req.pin)) {
        m_auditRepo.logAction(sess.user_id, "TRANSFER_FAILED", sess.primary_account_id, "Sai mã PIN giao dịch");
        res.message = "Mã PIN giao dịch không chính xác!";
        return res;
    }

    // 5. Kiểm tra hạn mức giao dịch
    if (req.amount_vnd <= 0 || req.amount_vnd > SINGLE_TXN_LIMIT_VND) {
        res.message = "Số tiền vượt quá hạn mức giao dịch tối đa cho 1 lần (500.000.000 VNĐ)!";
        return res;
    }

    std::string today = getCurrentDateStr();
    int64_t currentDailyTotal = m_txnRepo.getDailyTotalVnd(sess.user_id, today);
    if (currentDailyTotal + req.amount_vnd > DAILY_TXN_LIMIT_VND) {
        res.message = "Giao dịch thất bại! Đã vượt hạn mức cộng dồn trong ngày (1.000.000.000 VNĐ).";
        return res;
    }

    // 6. XÁC THỰC MÃ OTP (NẾU GIAO DỊCH > 50 TRIỆU VNĐ)
    if (req.amount_vnd > OTP_THRESHOLD_VND) {
        std::string otpError;
        if (!verifyOtp(sess.user_id, req.otp_challenge_id, req.otp, req.idempotency_key,
                       req.amount_vnd, receiverId, otpError)) {
            m_auditRepo.logAction(sess.user_id, "TRANSFER_FAILED", sess.primary_account_id, "Sai mã OTP");
            res.message = otpError;
            return res;
        }
    }

    // 7. Nạp Models từ DB lên RAM
    auto senderOpt = m_accRepo.findCheckingAccountById(sess.primary_account_id);
    std::optional<CheckingAccount> receiverOpt;
    if (req.is_interbank) receiverOpt = CheckingAccount(receiverId, "SIMULATED_EXTERNAL", 0, "ACTIVE", "");
    else receiverOpt = m_accRepo.findCheckingAccountById(receiverId);

    if (!senderOpt || !receiverOpt) {
        res.message = "Thông tin tài khoản giao dịch không tồn tại!";
        return res;
    }

    CheckingAccount sender = senderOpt.value();
    CheckingAccount receiver = receiverOpt.value();

    // 8. Khởi tạo Command & Thực thi biến đổi trên RAM
    const int64_t interbankFee = readFeeConfig(m_tursoDb, "admin.interbank_fee_vnd", 9900);
    const std::string description = req.is_interbank ? ("MÔ PHỎNG · " + req.receiver_bank + " · " + req.receiver_account_id) : req.description;
    TransferTransaction txn(sender, receiver, req.amount_vnd, description, req.is_interbank, interbankFee);
    if (!txn.process()) {
        res.message = "Số dư tài khoản không đủ để thực hiện giao dịch (bao gồm phí)!";
        return res;
    }

    int64_t newBalance = sender.getBalanceVnd();

    // 9. Đóng gói kết quả & Lưu Atomic Batch xuống TursoDB
    res.success = true;
    res.message = req.is_interbank
        ? "Đã ghi nhận chuyển tiền mô phỏng trong OOPBank; chưa gửi yêu cầu tới ngân hàng thụ hưởng."
        : "Chuyển tiền thành công!";
    res.transaction_id = txn.getTransactionId();
    res.post_balance_vnd = newBalance;

    std::string responseJsonStr = json(res).dump();
    bool ok = m_txnRepo.executeAtomicTransfer(txn, sess.user_id, today, req.idempotency_key, newBalance, responseJsonStr);

    if (ok) {
        m_auditRepo.logAction(sess.user_id, req.is_interbank ? "TRANSFER_INTERBANK_SIMULATED" : "TRANSFER_SUCCESS",
                              txn.getTransactionId(), "Amount: " + std::to_string(req.amount_vnd) +
                              " VND to " + receiverId + (req.is_interbank ? "; external bank request not sent" : ""));
    } else {
        if (const auto cached = m_txnRepo.getIdempotentResponse(req.idempotency_key, sess.user_id, "TRANSFER")) {
            if (cached->rfind("PENDING:", 0) == 0) {
                if (const auto recovered = recoveredTransaction(m_tursoDb, *cached, res.message)) return *recovered;
                res.message = "Yêu cầu chuyển tiền đang được xử lý; hãy gửi lại bằng chính mã yêu cầu này.";
                return res;
            }
            try { return json::parse(*cached).get<TransactionResult>(); } catch (...) {}
        }
        res = TransactionResult{false, "Lỗi CSDL khi thực thi giao dịch!", "", 0};
    }

    return res;
}

// =========================================================================
// 2. LUỒNG RÚT TIỀN (WithdrawTransaction)
// =========================================================================
TransactionResult TransactionService::executeWithdraw(const std::string& sessionToken, const std::string& pin,
                                                       const std::string& otp, const std::string& challengeId,
                                                       const std::string& idempotencyKey, int64_t amountVnd,
                                                       const std::string& desc, bool isAtmOffNet) {
    TransactionResult res;

    // 1. Xác thực Session
    auto sessOpt = m_sessionManager.getSession(sessionToken);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }
    const auto& sess = sessOpt.value();

    if (idempotencyKey.empty()) {
        res.message = "Thiếu mã idempotency cho yêu cầu rút tiền; hãy tạo mã duy nhất và giữ nguyên khi gửi lại.";
        return res;
    }
    if (const auto cached = m_txnRepo.getIdempotentResponse(idempotencyKey, sess.user_id, "WITHDRAW")) {
        if (cached->rfind("PENDING:", 0) == 0) {
            const std::string transactionId = cached->substr(8);
            const json transaction = m_tursoDb.execute(
                "SELECT amount_vnd, post_balance_vnd FROM transactions WHERE transaction_id = ? "
                "AND type IN ('WITHDRAW_ATM_OFFNET','WITHDRAW_COUNTER') AND status = 'SUCCESS' LIMIT 1;",
                json::array({{{"type", "text"}, {"value", transactionId}}}));
            try {
                const auto& rows = transaction["results"][0]["response"]["result"]["rows"];
                if (rows.is_array() && !rows.empty()) {
                    auto cellText = [](const json& cell) {
                        if (!cell.is_object() || !cell.contains("value") || cell["value"].is_null()) return std::string{};
                        const auto& value = cell["value"];
                        return value.is_string() ? value.get<std::string>() : value.dump();
                    };
                    return TransactionResult{true, "Rút tiền thành công!", transactionId,
                                             std::stoll(cellText(rows[0][1]))};
                }
            } catch (...) {}
            res.message = "Yêu cầu rút tiền đang được xử lý; hãy gửi lại bằng chính mã idempotency này.";
            return res;
        }
        try { return json::parse(*cached).get<TransactionResult>(); }
        catch (...) { res.message = "Không đọc được kết quả rút tiền đã lưu."; return res; }
    }

    // 2. XÁC THỰC MÃ PIN
    auto userOpt = m_userRepo.findById(sess.user_id);
    if (!userOpt || !userOpt->verifyPin(pin)) {
        m_auditRepo.logAction(sess.user_id, "WITHDRAW_FAILED", sess.primary_account_id, "Sai mã PIN");
        res.message = "Mã PIN giao dịch không chính xác!";
        return res;
    }

    // 3. XÁC THỰC OTP NẾU RÚT > 50 TRIỆU
    if (amountVnd > OTP_THRESHOLD_VND) {
        std::string otpError;
        if (!verifyOtp(sess.user_id, challengeId, otp, idempotencyKey, amountVnd, "CASH_WITHDRAW", otpError)) {
        res.message = "Mã OTP xác thực rút tiền giá trị lớn không chính xác!";
        return res;
        }
    }

    // 4. Nạp Model & Thực thi Command
    auto accOpt = m_accRepo.findCheckingAccountById(sess.primary_account_id);
    if (!accOpt) {
        res.message = "Tài khoản thanh toán không tồn tại!";
        return res;
    }

    CheckingAccount sourceAcc = accOpt.value();
    const int64_t atmFee = readFeeConfig(m_tursoDb, "admin.atm_fee_vnd", 3300);
    WithdrawTransaction txn(sourceAcc, amountVnd, desc, isAtmOffNet, atmFee);

    if (!txn.process()) {
        res.message = "Số dư không đủ để rút tiền (bao gồm phí)!";
        return res;
    }

    // 5. Reserve the idempotency key in the same database batch as the debit.
    res.success = true;
    res.message = "Rút tiền thành công!";
    res.transaction_id = txn.getTransactionId();
    res.post_balance_vnd = sourceAcc.getBalanceVnd();
    const bool ok = m_txnRepo.saveWithdrawBatch(txn, sourceAcc.getBalanceVnd(), "", "",
        idempotencyKey, sess.user_id, "WITHDRAW", json(res).dump());
    if (ok) {
        m_auditRepo.logAction(sess.user_id, "WITHDRAW_SUCCESS", txn.getTransactionId(), "Rút: " + std::to_string(amountVnd));
    } else {
        if (const auto cached = m_txnRepo.getIdempotentResponse(idempotencyKey, sess.user_id, "WITHDRAW")) {
            if (cached->rfind("PENDING:", 0) != 0) {
                try { return json::parse(*cached).get<TransactionResult>(); } catch (...) {}
            }
        }
        res.success = false;
        res.transaction_id.clear();
        res.post_balance_vnd = 0;
        res.message = "Lỗi CSDL khi ghi nhận rút tiền!";
    }

    return res;
}

// =========================================================================
// 3. LUỒNG THANH TOÁN HÓA ĐƠN (BillPaymentTransaction)
// =========================================================================
TransactionResult TransactionService::executeBillPayment(const std::string& sessionToken, const std::string& pin,
                                                          const std::string& billId, const std::string& desc) {
    TransactionResult res;

    // 1. Xác thực Session
    auto sessOpt = m_sessionManager.getSession(sessionToken);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }
    const auto& sess = sessOpt.value();

    // 2. XÁC THỰC MÃ PIN
    auto userOpt = m_userRepo.findById(sess.user_id);
    if (!userOpt || !userOpt->verifyPin(pin)) {
        m_auditRepo.logAction(sess.user_id, "BILL_PAY_FAILED", billId, "Sai mã PIN");
        res.message = "Mã PIN giao dịch không chính xác!";
        return res;
    }

    // 3. Nạp Model Account & Bill
    auto accOpt = m_accRepo.findCheckingAccountById(sess.primary_account_id);
    auto billOpt = m_billRepo.findById(billId);

    if (!accOpt || !billOpt) {
        res.message = "Tài khoản hoặc hóa đơn không tồn tại!";
        return res;
    }

    CheckingAccount payerAcc = accOpt.value();
    BillModel bill = billOpt.value();

    // 4. Thực thi Command
    BillPaymentTransaction txn(payerAcc, bill, "MÔ PHỎNG · " + desc);
    if (!txn.process()) {
        res.message = "Hóa đơn đã được thanh toán trước đó hoặc số dư không đủ!";
        return res;
    }

    // 5. Lưu Persistence
    bool ok = m_txnRepo.saveBillPaymentBatch(txn, payerAcc.getBalanceVnd());
    if (ok) {
        res.success = true;
        res.message = "Đã ghi nhận thanh toán hóa đơn mô phỏng trong OOPBank; chưa gửi xác nhận tới nhà cung cấp.";
        res.transaction_id = txn.getTransactionId();
        res.post_balance_vnd = payerAcc.getBalanceVnd();
        m_auditRepo.logAction(sess.user_id, "PAY_BILL_SIMULATED", billId,
                              "Bill: " + billId + "; provider settlement not sent");
    } else {
        res.message = "Lỗi CSDL khi cập nhật hóa đơn!";
    }

    return res;
}

// =========================================================================
// 4. LUỒNG NẠP TIỀN MẶT TẠI QUẦY (DepositTransaction)
// =========================================================================
CounterOperationResult TransactionService::executeDeposit(const std::string& tellerSessionToken, const std::string& targetAccId,
                                                            int64_t amountVnd, const std::string& desc,
                                                            const std::string& requestedExecutionId) {
    CounterOperationResult res;
    res.execution_id = requestedExecutionId.empty()
        ? "CASH_" + AuthSecurity::generateNumericCode(24) : requestedExecutionId;
    const auto session = m_sessionManager.getSession(tellerSessionToken);
    const std::string operatorId = session ? session->user_id : "SYSTEM";
    const auto fail = [&](const std::string& message) {
        res.message = message;
        res.trace_persisted = logCounterExecution(m_auditRepo, res.execution_id, operatorId,
            "DEPOSIT_COUNTER_FAILED", targetAccId, amountVnd, "", "FAILED", message);
        if (!res.trace_persisted) res.message += " Mã tra cứu " + res.execution_id + " chưa lưu được vào nhật ký.";
        return res;
    };
    if (const auto previous = previousCounterExecution(m_txnRepo, m_auditRepo, m_tursoDb,
            res.execution_id, operatorId, "COUNTER_DEPOSIT", "DEPOSIT_COUNTER_SUCCESS",
            "DEPOSIT_COUNTER_FAILED", "Nạp tiền mặt tại quầy thành công!")) return *previous;

    // Xác thực Session Teller/Giao dịch viên
    if (!session || (session->role != "TELLER" && session->role != "ADMIN")) {
        return fail("BẢO MẬT: Chỉ Giao dịch viên/Admin mới có quyền nạp tiền mặt tại quầy!");
    }
    const int64_t counterLimit = readFeeConfig(m_tursoDb, "counter.max_transaction_vnd", SINGLE_TXN_LIMIT_VND);
    if (amountVnd <= 0 || amountVnd > counterLimit) {
        return fail("Số tiền nạp vượt hạn mức mỗi giao dịch tại quầy (" + std::to_string(counterLimit) + " VNĐ).");
    }

    auto accOpt = m_accRepo.findCheckingAccountById(targetAccId);
    if (!accOpt) {
        return fail("Tài khoản thụ hưởng không tồn tại!");
    }

    CheckingAccount targetAcc = accOpt.value();
    if (!targetAcc.isActive()) return fail("Tài khoản không hoạt động.");
    const auto customer = m_userRepo.findById(targetAcc.getUserId());
    if (!customer || !customer->isActive()) return fail("Khách hàng không tồn tại hoặc đang bị khóa.");
    DepositTransaction txn(targetAcc, amountVnd, desc);

    if (!txn.process()) {
        return fail("Thao tác nạp tiền mặt thất bại!");
    }

    const CounterOperationResult successResult{true, "Nạp tiền mặt tại quầy thành công!", txn.getTransactionId(),
        targetAcc.getBalanceVnd(), res.execution_id, true};
    const bool ok = m_txnRepo.saveDepositBatch(txn, targetAcc.getBalanceVnd(), res.execution_id, operatorId,
                                               json(successResult).dump());
    if (ok) {
        res = successResult;
    } else {
        if (const auto previous = previousCounterExecution(m_txnRepo, m_auditRepo, m_tursoDb,
                res.execution_id, operatorId, "COUNTER_DEPOSIT", "DEPOSIT_COUNTER_SUCCESS",
                "DEPOSIT_COUNTER_FAILED", "Nạp tiền mặt tại quầy thành công!")) return *previous;
        return fail("Lỗi CSDL khi nạp tiền!");
    }
    if (!res.trace_persisted) res.message += " Mã tra cứu " + res.execution_id + " chưa lưu được vào nhật ký.";

    return res;
}

CounterOperationResult TransactionService::executeCounterWithdraw(const std::string& tellerSessionToken, const std::string& targetAccId,
                                                                   const std::string& customerPin, int64_t amountVnd,
                                                                   const std::string& requestedExecutionId) {
    CounterOperationResult res;
    res.execution_id = requestedExecutionId.empty()
        ? "CASH_" + AuthSecurity::generateNumericCode(24) : requestedExecutionId;
    const auto staff = m_sessionManager.getSession(tellerSessionToken);
    const std::string operatorId = staff ? staff->user_id : "SYSTEM";
    const auto fail = [&](const std::string& message) {
        res.message = message;
        res.trace_persisted = logCounterExecution(m_auditRepo, res.execution_id, operatorId,
            "COUNTER_WITHDRAW_FAILED", targetAccId, amountVnd, "", "FAILED", message);
        if (!res.trace_persisted) res.message += " Mã tra cứu " + res.execution_id + " chưa lưu được vào nhật ký.";
        return res;
    };
    if (const auto previous = previousCounterExecution(m_txnRepo, m_auditRepo, m_tursoDb,
            res.execution_id, operatorId, "COUNTER_WITHDRAW", "COUNTER_WITHDRAW_SUCCESS",
            "COUNTER_WITHDRAW_FAILED", "Rút tiền tại quầy thành công.")) return *previous;
    if (!staff || (staff->role != "TELLER" && staff->role != "ADMIN")) {
        return fail("Chỉ giao dịch viên/Admin được thực hiện rút tiền tại quầy.");
    }
    const int64_t counterLimit = readFeeConfig(m_tursoDb, "counter.max_transaction_vnd", SINGLE_TXN_LIMIT_VND);
    if (amountVnd <= 0 || amountVnd > counterLimit) return fail("Số tiền rút vượt hạn mức mỗi giao dịch tại quầy (" + std::to_string(counterLimit) + " VNĐ).");
    const auto account = m_accRepo.findCheckingAccountById(targetAccId);
    if (!account || !account->isActive()) return fail("Tài khoản không tồn tại hoặc không hoạt động.");
    const auto customer = m_userRepo.findById(account->getUserId());
    if (!customer || !customer->isActive()) return fail("Khách hàng không tồn tại hoặc đang bị khóa.");
    if (!customer->verifyPin(customerPin)) {
        return fail("Mã PIN giao dịch của khách hàng không chính xác.");
    }
    CheckingAccount source = *account;
    WithdrawTransaction txn(source, amountVnd, "Rút tiền mặt tại quầy", false, 0);
    if (!txn.process()) return fail("Số dư không đủ để rút tiền.");
    const CounterOperationResult successResult{true, "Rút tiền tại quầy thành công.", txn.getTransactionId(),
        source.getBalanceVnd(), res.execution_id, true};
    if (!m_txnRepo.saveWithdrawBatch(txn, source.getBalanceVnd(), res.execution_id, operatorId,
            res.execution_id, operatorId, "COUNTER_WITHDRAW", json(successResult).dump())) {
        if (const auto previous = previousCounterExecution(m_txnRepo, m_auditRepo, m_tursoDb,
                res.execution_id, operatorId, "COUNTER_WITHDRAW", "COUNTER_WITHDRAW_SUCCESS",
                "COUNTER_WITHDRAW_FAILED", "Rút tiền tại quầy thành công.")) return *previous;
        return fail("Không thể ghi nhận giao dịch rút tiền.");
    }
    res = successResult;
    return res;
}

bool TransactionService::recordCounterExecutionFailure(const std::string& requestedExecutionId,
                                                        const std::string& sessionToken,
                                                        const std::string& operation,
                                                        const std::string& targetAccId,
                                                        int64_t amountVnd,
                                                        const std::string& reason) {
    const std::string executionId = requestedExecutionId.empty()
        ? "CASH_" + AuthSecurity::generateNumericCode(24) : requestedExecutionId;
    const auto session = m_sessionManager.getSession(sessionToken);
    const std::string operatorId = session ? session->user_id : "SYSTEM";
    const bool isDeposit = operation == "DEPOSIT";
    return logCounterExecution(m_auditRepo, executionId, operatorId,
        isDeposit ? "DEPOSIT_COUNTER_FAILED" : "COUNTER_WITHDRAW_FAILED",
        targetAccId, amountVnd, "", "FAILED", reason);
}

TransactionResult TransactionService::executeTopup(const std::string& sessionToken, const std::string& pin, const std::string& carrier,
                                                    const std::string& phone, int64_t amountVnd, const std::string& idempotencyKey) {
    TransactionResult res;
    const auto session = m_sessionManager.getSession(sessionToken);
    if (!session || session->role != "CUSTOMER") { res.message = "Phiên khách hàng không hợp lệ."; return res; }
    if (idempotencyKey.empty()) { res.message = "Thiếu mã chống gửi trùng cho yêu cầu nạp điện thoại."; return res; }
    const std::vector<std::string> carriers{"VIETTEL", "MOBIFONE", "VINAPHONE"};
    if (std::find(carriers.begin(), carriers.end(), carrier) == carriers.end() || phone.size() != 10 ||
        !std::all_of(phone.begin(), phone.end(), [](unsigned char c){ return std::isdigit(c); }) || amountVnd < 10000 || amountVnd > 500000 || amountVnd % 10000 != 0) {
        res.message = "Nhà mạng, số điện thoại hoặc mệnh giá nạp không hợp lệ."; return res;
    }
    if (const auto cached = m_txnRepo.getIdempotentResponse(idempotencyKey, session->user_id, "MOBILE_TOPUP")) {
        if (cached->rfind("PENDING:", 0) == 0) {
            if (const auto recovered = recoveredTransaction(m_tursoDb, *cached, "Đã ghi nhận nạp tiền mô phỏng.")) return *recovered;
            res.message = "Yêu cầu nạp tiền đang được xử lý; hãy gửi lại bằng chính mã yêu cầu này.";
            return res;
        }
        try { return json::parse(*cached).get<TransactionResult>(); }
        catch (...) { res.message = "Không đọc được kết quả nạp tiền đã lưu."; return res; }
    }
    const auto user = m_userRepo.findById(session->user_id);
    if (!user || !user->verifyPin(pin)) { res.message = "Mã PIN giao dịch không chính xác."; return res; }
    auto account = m_accRepo.findCheckingAccountById(session->primary_account_id);
    if (!account || !account->isActive() || !account->canWithdraw(amountVnd)) { res.message = "Số dư khả dụng không đủ để nạp tiền."; return res; }
    const int64_t postBalance = account->getBalanceVnd() - amountVnd;
    res.success = true; res.message = "Đã ghi nhận nạp tiền mô phỏng."; res.transaction_id = "TXN_" + AuthSecurity::generateNumericCode(24); res.post_balance_vnd = postBalance;
    const std::string description = "MÔ PHỎNG NẠP " + carrier + " " + phone;
    if (!m_txnRepo.saveVirtualDebit(account->getAccountId(), session->user_id, amountVnd, "MOBILE_TOPUP", description, res.transaction_id,
                                    idempotencyKey, postBalance, json(res).dump())) {
        if (const auto cached = m_txnRepo.getIdempotentResponse(idempotencyKey, session->user_id, "MOBILE_TOPUP")) {
            if (cached->rfind("PENDING:", 0) == 0) {
                if (const auto recovered = recoveredTransaction(m_tursoDb, *cached, res.message)) return *recovered;
                res.success = false; res.transaction_id.clear(); res.post_balance_vnd = 0;
                res.message = "Yêu cầu nạp tiền đang được xử lý; hãy gửi lại bằng chính mã yêu cầu này.";
                return res;
            }
            try { return json::parse(*cached).get<TransactionResult>(); } catch (...) {}
        }
        res = TransactionResult{false, "Giao dịch chưa được ghi nhận do không đủ số dư hoặc lỗi database.", "", 0}; return res;
    }
    m_auditRepo.logAction(session->user_id, "MOBILE_TOPUP_SIMULATED", res.transaction_id, description + " · " + std::to_string(amountVnd));
    return res;
}

} // namespace Banking
