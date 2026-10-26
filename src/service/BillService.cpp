#include "services/BillService.hpp"
#include "model/TransactionModel.hpp"
#include "model/AccountModel.hpp"
#include "model/BillModel.hpp"
#include "core/AuthSecurity.hpp"
#include <array>
#include <ctime>

namespace Banking {

BillService::BillService(BillRepository& billRepo, UserRepository& userRepo,
                         AccountRepository& accRepo, TransactionRepository& txnRepo,
                         AuditRepository& auditRepo, SessionManager& sessionManager)
    : m_billRepo(billRepo), m_userRepo(userRepo), m_accRepo(accRepo),
      m_txnRepo(txnRepo), m_auditRepo(auditRepo), m_sessionManager(sessionManager) {}

GetBillProvidersResult BillService::getProviders(const std::string& sessionToken) {
    GetBillProvidersResult result;
    if (!m_sessionManager.getSession(sessionToken).has_value()) {
        result.message = "Phiên làm việc không hợp lệ hoặc đã hết hạn!";
        return result;
    }
    result.providers = {
        {"EVN_HANOI","EVN Hà Nội","ELECTRICITY"},{"EVN_HCMC","EVN TP. Hồ Chí Minh","ELECTRICITY"},{"EVN_CENTRAL","EVN Miền Trung","ELECTRICITY"},
        {"HANOI_WATER","Nước sạch Hà Nội","WATER"},{"SAWACO","SAWACO","WATER"},{"DANANG_WATER","Cấp nước Đà Nẵng","WATER"},
        {"VIETTEL_NET","Viettel Internet","INTERNET"},{"VNPT","VNPT","INTERNET"},{"FPT","FPT Telecom","INTERNET"},
        {"VIETTEL_POST","Viettel trả sau","MOBILE_POSTPAID"},{"MOBIFONE_POST","MobiFone trả sau","MOBILE_POSTPAID"},{"VINAPHONE_POST","VinaPhone trả sau","MOBILE_POSTPAID"}
    };
    result.success = true;
    result.message = "Đã tải nhà cung cấp hóa đơn.";
    return result;
}

// BƯỚC 1: TRA CỨU CƯỚC HÓA ĐƠN
QueryBillResult BillService::queryBill(const QueryBillRequest& req) {
    QueryBillResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc không hợp lệ hoặc đã hết hạn!";
        return res;
    }

    const auto providers = getProviders(req.session_token).providers;
    const auto provider = std::find_if(providers.begin(), providers.end(), [&](const BillProviderDTO& p){ return p.provider_id == req.provider_id; });
    if (provider == providers.end() || req.customer_code.empty() || req.customer_code.size() > 32) {
        res.message = "Nhà cung cấp hoặc mã khách hàng không hợp lệ."; return res;
    }
    auto billOpt = m_billRepo.findUnpaidBill(req.provider_id, req.customer_code);
    if (!billOpt) {
        const std::time_t now = std::time(nullptr); std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &now);
#else
        gmtime_r(&now, &tm);
#endif
        char period[8]; std::strftime(period, sizeof(period), "%Y-%m", &tm);
        const std::string seed = AuthSecurity::hashPin(req.provider_id + ":" + req.customer_code + ":" + period);
        const int64_t amount = 50000 + (std::stoll(seed.substr(0, 7), nullptr, 16) % 450000);
        std::string numericId;
        numericId.reserve(40);
        for (const char ch : seed.substr(0, 20)) {
            numericId += ch >= '0' && ch <= '9'
                ? std::string(1, ch)
                : std::to_string(10 + ch - 'a');
        }
        const std::string billId = "SIMBILL_" + numericId;
        if (!m_billRepo.createSimulatedBill(billId, provider->provider_id, provider->provider_name, req.customer_code,
                                             "Khách hàng " + req.customer_code, amount, provider->service_type, period)) {
            res.message = "Không tạo được hóa đơn mô phỏng trong database."; return res;
        }
        billOpt = m_billRepo.findUnpaidBill(req.provider_id, req.customer_code);
    }
    if (!billOpt.has_value()) {
        res.message = "Không tìm thấy hóa đơn chưa thanh toán!";
        return res;
    }

    const auto& bill = billOpt.value();
    res.success = true;
    res.message = "Tra cứu cước thành công!";
    res.bill = BillDTO{
        bill.getBillId(), bill.getProviderId(), bill.getProviderName(),
        bill.getCustomerCode(), bill.getCustomerName(), bill.getAmountVnd(),
        bill.getServiceType(), bill.getStatus(), bill.getDueDate(), bill.getPeriod()
    };
    return res;
}

// BƯỚC 2 & 3: XÁC THỰC PIN & THANH TOÁN
PayBillResult BillService::payBill(const PayBillRequest& req) {
    PayBillResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }
    const auto& sess = sessOpt.value();

    if (req.idempotency_key.empty()) {
        res.message = "Thiếu mã chống gửi trùng cho yêu cầu thanh toán hóa đơn.";
        return res;
    }
    if (const auto cached = m_txnRepo.getIdempotentResponse(req.idempotency_key, sess.user_id, "BILL_PAYMENT")) {
        if (cached->rfind("PENDING:", 0) == 0) {
            res.message = "Thanh toán đang được xử lý; hãy gửi lại bằng chính mã yêu cầu này.";
            return res;
        }
        try { return json::parse(*cached).get<PayBillResult>(); }
        catch (...) { res.message = "Không đọc được kết quả thanh toán đã lưu."; return res; }
    }

    // 1. Xác thực PIN trên RAM Cache
    auto userOpt = m_userRepo.findById(sess.user_id);
    if (!userOpt|| !userOpt->verifyPin(req.pin)) {
        m_auditRepo.logAction(sess.user_id, "PAY_BILL_FAILED", req.bill_id, "Invalid PIN");
        res.message = "Mã PIN giao dịch không chính xác!";
        return res;
    }

    // 2. Kiểm tra trạng thái Hóa đơn
    auto billOpt = m_billRepo.findById(req.bill_id);
    if (!billOpt.has_value() || !billOpt->isPayable()) {
        res.message = "Hóa đơn không hợp lệ hoặc đã được thanh toán trước đó!";
        return res;
    }
    BillModel bill = billOpt.value(); // Lấy bản sao để xử lý

    // 3. Truy vấn cụ thể Model Tài khoản từ Repository
    auto accOpt = m_accRepo.findCheckingAccountById(sess.primary_account_id);
    if (!accOpt.has_value()) {
        res.message = "Không tìm thấy tài khoản thanh toán hợp lệ!";
        return res;
    }
    CheckingAccount payerAcc = accOpt.value();

    // 4. Kiểm tra số dư
    if (payerAcc.getBalanceVnd() < bill.getAmountVnd()) {
        res.message = "Số dư tài khoản không đủ để thanh toán!";
        return res;
    }

    // 5. Khởi tạo đối tượng Đa hình BillPaymentTransaction
    std::string desc = "MÔ PHỎNG · Thanh toán cước " + bill.getServiceType() + " · HĐ " + bill.getBillId();
    BillPaymentTransaction txn(payerAcc, bill, desc);

    if (!txn.process()) {
        res.message = "Lỗi xử lý logic hóa đơn (Số dư không hợp lệ)!";
        return res;
    }

    // 6. Lưu giao dịch theo Batch Atomic vào Turso DB
    res.success = true;
    res.message = "Đã ghi nhận thanh toán hóa đơn mô phỏng trong OOPBank; chưa gửi xác nhận tới nhà cung cấp.";
    res.transaction_id = txn.getTransactionId();
    res.post_balance_vnd = payerAcc.getBalanceVnd();
    if (m_txnRepo.saveBillPaymentBatch(txn, payerAcc.getBalanceVnd(), sess.user_id,
                                       req.idempotency_key, json(res).dump())) {
        m_auditRepo.logAction(sess.user_id, "PAY_BILL_SIMULATED", bill.getBillId(),
                              "Amount: " + std::to_string(bill.getAmountVnd()) + " VND; provider settlement not sent");
    } else {
        if (const auto cached = m_txnRepo.getIdempotentResponse(req.idempotency_key, sess.user_id, "BILL_PAYMENT")) {
            if (cached->rfind("PENDING:", 0) == 0) {
                res.success = false;
                res.transaction_id.clear();
                res.post_balance_vnd = 0;
                res.message = "Thanh toán đang được xử lý; hãy gửi lại bằng chính mã yêu cầu này.";
                return res;
            }
            try { return json::parse(*cached).get<PayBillResult>(); } catch (...) {}
        }
        res.success = false;
        res.transaction_id.clear();
        res.post_balance_vnd = 0;
        res.message = "Lỗi CSDL khi thực thi thanh toán!";
    }

    return res;
}

} // namespace Banking
