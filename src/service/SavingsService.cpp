#include "services/SavingsService.hpp"
#include <algorithm>
#include <ctime>
#include "core/AuthSecurity.hpp"

namespace Banking {

namespace {
bool allStatementsSucceeded(const json& response) {
    if (!response.contains("results") || !response["results"].is_array() || response["results"].empty()) return false;
    for (const auto& result : response["results"]) {
        if (!result.is_object() || result.value("type", "") != "ok") return false;
    }
    return true;
}

double readSavingsRateCap(TursoDB& database) {
    const json response = database.execute(
        "SELECT value FROM system_config WHERE key = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", "admin.savings_max_rate_percent"}}}));
    try {
        const auto cell = response["results"][0]["response"]["result"]["rows"][0][0];
        if (!cell.is_object() || !cell.contains("value") || cell["value"].is_null()) return 6.5;
        const auto& value = cell["value"];
        const double rate = std::stod(value.is_string() ? value.get<std::string>() : value.dump());
        return rate >= 0.0 && rate <= 50.0 ? rate : 6.5;
    } catch (...) {
        return 6.5;
    }
}

double savingsProductRate(int termMonths, int termDays) {
    if (termDays == 7 || termDays == 14 || termDays == 21) return 0.5;
    if (termDays != 0) return 0.0;
    if (termMonths == 1) return 4.5;
    if (termMonths == 2) return 4.6;
    if (termMonths >= 3 && termMonths <= 5) return 4.7;
    if (termMonths >= 6 && termMonths <= 11) return 5.3;
    if (termMonths == 13 || termMonths == 15 || termMonths == 18) return 5.85;
    if (termMonths == 24 || termMonths == 36 || termMonths == 48 || termMonths == 60) return 6.5;
    return 0.0;
}

} // namespace



SavingsService::SavingsService(SavingsRepository& savingsRepo, AccountRepository& accRepo,
                               UserRepository& userRepo, AuditRepository& auditRepo,
                               SessionManager& sessionManager, TursoDB& tursoDb)
    : m_savingsRepo(savingsRepo), m_accRepo(accRepo), m_userRepo(userRepo),
      m_auditRepo(auditRepo), m_sessionManager(sessionManager), m_tursoDb(tursoDb) {}

// =========================================================================
// 1. MỞ SỔ TIẾT KIỆM
// =========================================================================
SavingsOperationResult SavingsService::openSavings(const OpenSavingsRequest& req) {
    SavingsOperationResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }
    const auto& sess = sessOpt.value();

    const std::string targetUserId = req.target_user_id.empty() ? sess.user_id : req.target_user_id;
    const bool staffSession = sess.role == "TELLER" || sess.role == "ADMIN";
    if (sess.role != "CUSTOMER" && !staffSession) {
        res.message = "Bạn không có quyền mở sổ tiết kiệm.";
        return res;
    }
    if (sess.role == "CUSTOMER" && targetUserId != sess.user_id) {
        res.message = "Khách hàng chỉ được mở sổ tiết kiệm cho chính mình.";
        return res;
    }
    if (staffSession && req.target_user_id.empty()) {
        res.message = "Thiếu thông tin khách hàng cần mở sổ tiết kiệm.";
        return res;
    }
    if (staffSession && !req.kyc_verified) {
        res.message = "Cần xác nhận đã đối chiếu CCCD bản gốc trước khi mở sổ tiết kiệm.";
        return res;
    }
    const std::string fundingMethod = req.funding_method.empty() ? "ACCOUNT" : req.funding_method;
    if ((fundingMethod != "ACCOUNT" && fundingMethod != "COUNTER_CASH") ||
        (!staffSession && fundingMethod != "ACCOUNT")) {
        res.message = "Nguồn tiền gửi không hợp lệ hoặc chỉ Teller/Admin mới được nhận tiền mặt tại quầy.";
        return res;
    }

    auto userOpt = m_userRepo.findById(targetUserId);
    if (!userOpt || userOpt->getRole() != "CUSTOMER" || !userOpt->isActive()) {
        res.message = "Khách hàng không tồn tại hoặc không hoạt động.";
        return res;
    }
    if (req.idempotency_key.empty()) {
        res.message = "Thiếu mã chống gửi trùng cho yêu cầu mở sổ tiết kiệm.";
        return res;
    }
    const std::string idempotencyKey = targetUserId + ":OPEN_SAVINGS:" + req.idempotency_key;
    const json cached = m_tursoDb.execute(
        "SELECT response_json FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = 'OPEN_SAVINGS' LIMIT 1;",
        json::array({{{"type", "text"}, {"value", idempotencyKey}},
                     {{"type", "text"}, {"value", targetUserId}}}));
    try {
        const auto value = cached["results"][0]["response"]["result"]["rows"][0][0]["value"];
        const std::string response = value.is_string() ? value.get<std::string>() : value.dump();
        if (response.rfind("PENDING:", 0) == 0) {
            res.message = "Yêu cầu mở sổ đang được xử lý; hãy gửi lại bằng chính mã yêu cầu này.";
            return res;
        }
        const auto previous = json::parse(response).get<SavingsOperationResult>();
        if (previous.success) return previous;
    } catch (...) {
        // There is no saved result for this key yet.
    }
    if (!userOpt->verifyPin(req.pin)) {
        res.message = "Mã PIN giao dịch không chính xác!";
        return res;
    }

    const bool validTerm = (req.term_days == 0 && savingsProductRate(req.term_months, 0) > 0.0) ||
                           ((req.term_months == 1) && savingsProductRate(0, req.term_days) > 0.0);
    if (req.amount_vnd < 1000000 || !validTerm) {
        res.message = "Số tiền hoặc kỳ hạn gửi không hợp lệ.";
        return res;
    }
    if (req.maturity_option != "RENEW_ALL" && req.maturity_option != "RENEW_PRINCIPAL" && req.maturity_option != "CLOSE_TO_ACCOUNT") {
        res.message = "Phương thức đáo hạn không hợp lệ.";
        return res;
    }

    const std::string sourceAccountId = req.source_account_id.empty()
        ? sess.primary_account_id : req.source_account_id;
    if (sourceAccountId.empty()) {
        res.message = "Khách hàng chưa có tài khoản thanh toán để trích tiền.";
        return res;
    }
    const auto accounts = m_accRepo.getAccountsByUserId(targetUserId);
    const auto sourceAccount = std::find_if(accounts.begin(), accounts.end(), [&](const AccountDTO& account) {
        return account.account_id == sourceAccountId && account.account_type == "CHECKING" &&
               account.status == "ACTIVE";
    });
    if (sourceAccount == accounts.end()) {
        res.message = "Tài khoản nguồn không phải tài khoản thanh toán đang hoạt động của khách hàng.";
        return res;
    }

    const int64_t currentBal = m_accRepo.getBalance(sourceAccountId);
    if (fundingMethod == "ACCOUNT" && currentBal < req.amount_vnd) {
        res.message = "Số dư tài khoản thanh toán không đủ!";
        return res;
    }

    const double productRate = savingsProductRate(req.term_months, req.term_days);
    const double rate = std::min(productRate, readSavingsRateCap(m_tursoDb));
    std::string savingsId = "SAV_" + AuthSecurity::generateNumericCode(24);

    std::vector<std::pair<std::string, json>> batch;
    const std::string reservation = "PENDING:" + savingsId;
    batch.push_back({
        "INSERT INTO idempotency_keys (idempotency_key, user_id, action, response_json, created_at) "
        "VALUES (?, ?, 'OPEN_SAVINGS', ?, CURRENT_TIMESTAMP) ON CONFLICT(idempotency_key) DO NOTHING;",
        json::array({{{"type", "text"}, {"value", idempotencyKey}},
                     {{"type", "text"}, {"value", targetUserId}},
                     {{"type", "text"}, {"value", reservation}}})
    });
    const std::string savingsTransactionId = "TXN_" + AuthSecurity::generateNumericCode(24);
    if (fundingMethod == "ACCOUNT") {
        const std::string sqlDeduct = "UPDATE accounts SET balance_vnd = balance_vnd - ? WHERE account_id = ? AND user_id = ? AND account_type = 'CHECKING' AND status = 'ACTIVE' AND balance_vnd >= ? AND changes() = 1 AND EXISTS (SELECT 1 FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = 'OPEN_SAVINGS' AND response_json = ?);";
        const json argsDeduct = json::array({
            {{"type", "numeric"}, {"value", req.amount_vnd}},
            {{"type", "text"}, {"value", sourceAccountId}},
            {{"type", "text"}, {"value", targetUserId}},
            {{"type", "numeric"}, {"value", req.amount_vnd}},
            {{"type", "text"}, {"value", idempotencyKey}},
            {{"type", "text"}, {"value", targetUserId}},
            {{"type", "text"}, {"value", reservation}}
        });
        batch.push_back({sqlDeduct, argsDeduct});
    } else {
        // Claim the customer's active checking account without changing its balance.
        // This makes the following INSERT conditional on a fresh account ownership/status check.
        batch.push_back({
            "UPDATE accounts SET balance_vnd = balance_vnd WHERE account_id = ? AND user_id = ? AND account_type = 'CHECKING' AND status = 'ACTIVE' AND changes() = 1 AND EXISTS (SELECT 1 FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = 'OPEN_SAVINGS' AND response_json = ?);",
            json::array({{{"type", "text"}, {"value", sourceAccountId}},
                         {{"type", "text"}, {"value", targetUserId}},
                         {{"type", "text"}, {"value", idempotencyKey}},
                         {{"type", "text"}, {"value", targetUserId}},
                         {{"type", "text"}, {"value", reservation}}})
        });
    }

    // Thêm sổ tiết kiệm mới
    std::string sqlInsert =
        "INSERT INTO savings_accounts (savings_id, user_id, source_account_id, principal_vnd, "
        "interest_rate, term_months, term_days, maturity_option, status, start_date, maturity_date, created_at) "
        "SELECT ?, ?, ?, ?, ?, ?, ?, ?, 'ACTIVE', CURRENT_TIMESTAMP, datetime(CURRENT_TIMESTAMP, ?), CURRENT_TIMESTAMP "
        "WHERE changes() = 1;";
    const int storedTermMonths = req.term_days > 0 ? 1 : req.term_months;
    const std::string maturityModifier = "+" + std::to_string(req.term_days > 0 ? req.term_days : req.term_months) +
                                         (req.term_days > 0 ? " days" : " months");
    json argsInsert = json::array({
        {{"type", "text"},    {"value", savingsId}},
        {{"type", "text"},    {"value", targetUserId}},
        {{"type", "text"},    {"value", sourceAccountId}},
        {{"type", "numeric"}, {"value", req.amount_vnd}},
        {{"type", "numeric"}, {"value", rate}},
        {{"type", "numeric"}, {"value", storedTermMonths}},
        {{"type", "numeric"}, {"value", req.term_days}},
        {{"type", "text"},    {"value", req.maturity_option}},
        {{"type", "text"},    {"value", maturityModifier}}
    });
    batch.push_back({sqlInsert, argsInsert});

    batch.push_back({
        "INSERT INTO transactions (transaction_id, sender_account_id, receiver_account_id, amount_vnd, fee_vnd, type, status, description, post_balance_vnd, created_at) "
        "SELECT ?, ?, ?, ?, 0, 'SAVINGS_OPEN', 'SUCCESS', ?, balance_vnd, CURRENT_TIMESTAMP FROM accounts "
        "WHERE account_id = ? AND changes() = 1;",
        json::array({{{"type", "text"}, {"value", savingsTransactionId}},
                     {{"type", "text"}, {"value", fundingMethod == "COUNTER_CASH" ? "COUNTER_CASH" : sourceAccountId}},
                     {{"type", "text"}, {"value", savingsId}},
                     {{"type", "numeric"}, {"value", req.amount_vnd}},
                     {{"type", "text"}, {"value", fundingMethod == "COUNTER_CASH" ? "Nộp tiền mặt mở sổ tiết kiệm " + savingsId : "Trích tài khoản mở sổ tiết kiệm " + savingsId}},
                     {{"type", "text"}, {"value", sourceAccountId}}})
    });

    res.success = true;
    res.message = "Mở sổ tiết kiệm thành công!";
    res.savings_id = savingsId;
    batch.push_back({
        "UPDATE idempotency_keys SET response_json = ? WHERE idempotency_key = ? AND user_id = ? AND action = 'OPEN_SAVINGS' "
        "AND response_json = ? AND EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
        json::array({{{"type", "text"}, {"value", json(res).dump()}},
                     {{"type", "text"}, {"value", idempotencyKey}},
                     {{"type", "text"}, {"value", targetUserId}},
                     {{"type", "text"}, {"value", reservation}},
                     {{"type", "text"}, {"value", savingsTransactionId}}})
    });
    batch.push_back({
        "DELETE FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = 'OPEN_SAVINGS' "
        "AND response_json = ? AND NOT EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
        json::array({{{"type", "text"}, {"value", idempotencyKey}},
                     {{"type", "text"}, {"value", targetUserId}},
                     {{"type", "text"}, {"value", reservation}},
                     {{"type", "text"}, {"value", savingsTransactionId}}})
    });

    json batchRes = m_tursoDb.executeBatch(batch);
    const json persisted = m_tursoDb.execute(
        "SELECT 1 FROM savings_accounts s JOIN transactions t ON t.receiver_account_id = s.savings_id "
        "WHERE s.savings_id = ? AND s.user_id = ? AND t.transaction_id = ? AND t.status = 'SUCCESS' LIMIT 1;",
        json::array({{{"type", "text"}, {"value", savingsId}},
                     {{"type", "text"}, {"value", targetUserId}},
                     {{"type", "text"}, {"value", savingsTransactionId}}}));
    bool saved = false;
    try { saved = !persisted["results"][0]["response"]["result"]["rows"].empty(); } catch (...) {}
    if (allStatementsSucceeded(batchRes) && saved) {
        res.success = true;
        res.message = "Mở sổ tiết kiệm thành công!";
        res.savings_id = savingsId;
        m_auditRepo.logAction(sess.user_id, "OPEN_SAVINGS_SUCCESS", savingsId,
                              "Customer: " + targetUserId + ", amount: " + std::to_string(req.amount_vnd) +
                              " VND, funding: " + fundingMethod);
    } else {
        const json previous = m_tursoDb.execute(
            "SELECT response_json FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = 'OPEN_SAVINGS' LIMIT 1;",
            json::array({{{"type", "text"}, {"value", idempotencyKey}},
                         {{"type", "text"}, {"value", targetUserId}}}));
        try {
            const auto value = previous["results"][0]["response"]["result"]["rows"][0][0]["value"];
            const std::string response = value.is_string() ? value.get<std::string>() : value.dump();
            if (response.rfind("PENDING:", 0) == 0) {
                res.success = false;
                res.savings_id.clear();
                res.message = "Yêu cầu mở sổ đang được xử lý; hãy gửi lại bằng chính mã yêu cầu này.";
                return res;
            }
            const auto result = json::parse(response).get<SavingsOperationResult>();
            if (result.success) return result;
        } catch (...) {}
        res.success = false;
        res.savings_id.clear();
        res.message = "Lỗi CSDL khi mở sổ tiết kiệm!";
    }

    return res;
}

// =========================================================================
// 2. TẤT TOÁN SỔ TIẾT KIỆM
// =========================================================================
SavingsOperationResult SavingsService::closeSavings(const CloseSavingsRequest& req) {
    SavingsOperationResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc hết hạn!";
        return res;
    }
    const auto& sess = sessOpt.value();

    // Xác thực PIN trước khi tất toán
    auto userOpt = m_userRepo.findById(sess.user_id);
    if (!userOpt || !userOpt->verifyPin(req.pin)) {
        res.message = "Mã PIN giao dịch không chính xác!";
        return res;
    }

    auto savingsOpt = m_savingsRepo.findById(req.savings_id);
    if (!savingsOpt || savingsOpt->getUserId() != sess.user_id) {
        res.message = "Sổ tiết kiệm không tồn tại hoặc không thuộc tài khoản của bạn.";
        return res;
    }
    const auto& sav = savingsOpt.value();
    if (!sav.isActive()) {
        const json previous = m_tursoDb.execute(
            "SELECT amount_vnd FROM transactions WHERE sender_account_id = ? AND type = 'SAVINGS_CLOSE' "
            "AND status = 'SUCCESS' ORDER BY created_at DESC LIMIT 1;",
            json::array({{{"type", "text"}, {"value", sav.getAccountId()}}}));
        try {
            const auto& row = previous["results"][0]["response"]["result"]["rows"][0];
            const auto payout = std::stoll(row[0]["value"].is_string()
                ? row[0]["value"].get<std::string>() : row[0]["value"].dump());
            res.success = true;
            res.message = "Sổ tiết kiệm đã được tất toán trước đó.";
            res.savings_id = sav.getAccountId();
            res.payout_total_vnd = payout;
            res.interest_earned_vnd = std::max<int64_t>(0, payout - sav.getOriginalAmountVnd());
            return res;
        } catch (...) {
            res.message = "Sổ tiết kiệm đã tất toán; không tìm thấy giao dịch để khôi phục kết quả.";
            return res;
        }
    }

    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char todayBuffer[11]; std::strftime(todayBuffer, sizeof(todayBuffer), "%Y-%m-%d", &utc);
    const bool early = std::string(todayBuffer) < sav.getMaturityDate();
    const json heldResponse = m_tursoDb.execute("SELECT CAST(julianday('now') - julianday(start_date) AS INTEGER) FROM savings_accounts WHERE savings_id = ? AND user_id = ? AND status = 'ACTIVE';",
        json::array({{{"type","text"},{"value",sav.getAccountId()}},{{"type","text"},{"value",sess.user_id}}}));
    int actualDaysHeld = 0;
    try { actualDaysHeld = std::max(0, std::stoi(heldResponse["results"][0]["response"]["result"]["rows"][0][0]["value"].get<std::string>())); } catch (...) {}
    const int64_t interestVnd = early ? sav.calculateEarlyWithdrawalInterest(actualDaysHeld) : sav.calculateMaturityInterest();
    int64_t totalPayout = sav.getOriginalAmountVnd() + interestVnd;

    std::vector<std::pair<std::string, json>> batch;

    // Cộng tổng tiền (Gốc + Lãi) vào tài khoản thanh toán
    std::string sqlCredit = "UPDATE accounts SET balance_vnd = balance_vnd + ? WHERE account_id = ? AND status = 'ACTIVE' AND changes() = 1;";
    json argsCredit = json::array({
        {{"type", "numeric"}, {"value", totalPayout}},
        {{"type", "text"},    {"value", sess.primary_account_id}}
    });
    batch.push_back({sqlCredit, argsCredit});

    // Cập nhật trạng thái sổ thành CLOSED
    std::string sqlClose = "UPDATE savings_accounts SET status = ? WHERE savings_id = ? AND user_id = ? AND status = 'ACTIVE' AND maturity_date = ? AND EXISTS (SELECT 1 FROM accounts WHERE account_id = ? AND status = 'ACTIVE');";
    json argsClose = json::array({{{"type", "text"}, {"value", early ? "EARLY_WITHDRAWN" : "CLOSED"}},
                                  {{"type", "text"}, {"value", sav.getAccountId()}}, {{"type", "text"}, {"value", sess.user_id}},
                                  {{"type", "text"}, {"value", sav.getMaturityDate()}},
                                  {{"type", "text"}, {"value", sess.primary_account_id}}});
    // Claim the savings record before crediting; a concurrent close can only claim it once.
    batch.clear();
    batch.push_back({sqlClose, argsClose});
    batch.push_back({sqlCredit, argsCredit});
    const std::string closeTransactionId = "TXN_" + AuthSecurity::generateNumericCode(24);
    batch.push_back({
        "INSERT INTO transactions (transaction_id,sender_account_id,receiver_account_id,amount_vnd,fee_vnd,type,status,description,post_balance_vnd,created_at) "
        "SELECT ?, ?, ?, ?, 0, 'SAVINGS_CLOSE', 'SUCCESS', ?, balance_vnd, CURRENT_TIMESTAMP FROM accounts WHERE account_id = ? AND changes() = 1;",
        json::array({{{"type","text"},{"value",closeTransactionId}},{{"type","text"},{"value",sav.getAccountId()}},
                     {{"type","text"},{"value",sess.primary_account_id}},{{"type","numeric"},{"value",totalPayout}},
                     {{"type","text"},{"value",early ? "Tất toán tiết kiệm trước hạn" : "Tất toán tiết kiệm đúng hạn"}},
                     {{"type","text"},{"value",sess.primary_account_id}}})
    });

    json batchRes = m_tursoDb.executeBatch(batch);
    const json recorded = m_tursoDb.execute("SELECT 1 FROM transactions WHERE transaction_id = ? LIMIT 1;",
        json::array({{{"type","text"},{"value",closeTransactionId}}}));
    bool transactionRecorded = false;
    try { transactionRecorded = !recorded["results"][0]["response"]["result"]["rows"].empty(); } catch (...) {}
    if (allStatementsSucceeded(batchRes) && transactionRecorded) {
        res.success = true;
        res.message = early ? "Tất toán trước hạn thành công; lãi tính theo lãi suất không kỳ hạn." : "Tất toán đúng hạn thành công.";
        res.savings_id = sav.getAccountId();
        res.payout_total_vnd = totalPayout;
        res.interest_earned_vnd = interestVnd;
        m_auditRepo.logAction(sess.user_id, "CLOSE_SAVINGS_SUCCESS", sav.getAccountId(), "Total Payout: " + std::to_string(totalPayout) + " VND");
    } else {
        res.message = "Lỗi CSDL khi tất toán!";
    }

    return res;
}

// =========================================================================
// 3. XEM DANH SÁCH SỔ TIẾT KIỆM (TÍNH TỔNG GỐC VÀ TỔNG LÃI DỰ KIẾN)
// =========================================================================
GetSavingsListResult SavingsService::getSavingsList(const GetSavingsListRequest& req) {
    GetSavingsListResult res;

    // 1. Xác thực phiên làm việc
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn hoặc không hợp lệ!";
        return res;
    }
    const auto& sess = sessOpt.value();

    // 2. Xác định Target User ID (Tự tra cứu hoặc Teller/Admin tra cứu giúp khách)
    std::string targetUserId = req.target_user_id.empty() ? sess.user_id : req.target_user_id;

    if (sess.role == "CUSTOMER" && targetUserId != sess.user_id) {
        res.message = "Lỗi phân quyền: Khách hàng chỉ được tra cứu sổ tiết kiệm của chính mình!";
        return res;
    }

    std::string filterStatus = req.status_filter.empty() ? "ACTIVE" : req.status_filter;

    // 3. Truy vấn danh sách từ Repository
    SavingsBook book = m_savingsRepo.findSavingsBookByUserId(targetUserId, filterStatus);
    const auto& accounts = book.getList();

    int64_t sumPrincipal = 0;
    int64_t sumExpectedInterest = 0;

    for (const auto& acc : accounts) {
        SavingsDTO dto;
        dto.savings_id = acc.getAccountId();
        dto.user_id = targetUserId;
        dto.source_account_id = sess.primary_account_id;
        dto.principal_vnd = acc.getOriginalAmountVnd();
        dto.interest_rate = acc.getInterestRate();
        dto.term_months = acc.getTermMonths();
        dto.term_days = acc.getTermDays();
        
        const double termYears = dto.term_days > 0
            ? static_cast<double>(dto.term_days) / 365.0
            : static_cast<double>(dto.term_months) / 12.0;
        dto.expected_interest_vnd = static_cast<int64_t>(
            dto.principal_vnd * (dto.interest_rate / 100.0) * termYears
        );
        
        dto.maturity_option = acc.getMaturityOption();
        dto.status = acc.getStatus();
        dto.start_date = ""; // Lấy từ DB nếu cần
        dto.maturity_date = acc.getMaturityDate();

        // Cộng dồn tổng gốc và tổng lãi dự kiến
        sumPrincipal += dto.principal_vnd;
        sumExpectedInterest += dto.expected_interest_vnd;

        res.items.push_back(dto);
    }

    res.success = true;
    res.message = "Truy vấn danh sách sổ tiết kiệm thành công!";
    res.total_principal_vnd = sumPrincipal;
    res.total_expected_interest_vnd = sumExpectedInterest;
    return res;
}

} // namespace Banking
