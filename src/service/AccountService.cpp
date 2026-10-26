#include "services/AccountService.hpp"
#include "core/AuthSecurity.hpp"

namespace Banking {

AccountService::AccountService(AccountRepository& accountRepo, UserRepository& userRepo, AuditRepository& auditRepo)
    : m_accountRepo(accountRepo), m_userRepo(userRepo), m_auditRepo(auditRepo) {}

std::string AccountService::generateNewAccountId(const std::string& type) {
    if (type == "CREDIT") return "CRE_" + AuthSecurity::generateNumericCode(20);
    if (type == "LOAN")   return "LON_" + AuthSecurity::generateNumericCode(20);
    if (type == "SAVINGS") return "SAV_" + AuthSecurity::generateNumericCode(20);
    
    return "ACC_" + AuthSecurity::generateNumericCode(20);
}

// =========================================================================
// TÍNH NĂNG 1: XEM DANH SÁCH & SỐ DƯ TÀI KHOẢN (RBAC ENFORCEMENT)
// =========================================================================
GetAccountsResult AccountService::getAccounts(const std::string& operatorUserId, const std::string& targetUserId) {
    GetAccountsResult result;

    // 1. Nạp thông tin người thực hiện thao tác
    std::shared_ptr<User> operatorUser = m_userRepo.findById(operatorUserId);
    if (!operatorUser) {
        result.success = false;
        result.message = "Người dùng thực hiện không tồn tại trên hệ thống!";
        return result;
    }

    // 2. Kiểm tra RBAC: Khách hàng CHỈ ĐƯỢC XEM chính chủ. GDV/Admin xem được tất cả.
    if (operatorUser->getRole() == "CUSTOMER" && operatorUserId != targetUserId) {
        result.success = false;
        result.message = "Lỗi phân quyền: Khách hàng chỉ được phép truy vấn tài khoản chính chủ!";
        return result;
    }

    // 3. Truy vấn dữ liệu từ Repository
    try {
        result.accounts = m_accountRepo.getAccountsByUserId(targetUserId);
        result.success = true;
        result.message = "Truy vấn danh sách tài khoản thành công!";
    } catch (const std::exception& error) {
        result.success = false;
        result.message = error.what();
    }

    return result;
}

// =========================================================================
// TÍNH NĂNG 2: TRA CỨU LỊCH SỬ GIAO DỊCH / IN SAO KÊ TÀI KHOẢN
// =========================================================================
TransactionHistoryResult AccountService::getTransactionHistory(const std::string& operatorUserId, 
                                                              const std::string& stk, 
                                                              int limit, 
                                                              int page) {
    TransactionHistoryResult result;
    result.current_page = page;

    // 1. Xác thực người thao tác
    std::shared_ptr<User> operatorUser = m_userRepo.findById(operatorUserId);
    if (!operatorUser) {
        result.success = false;
        result.message = "Người dùng thao tác không tồn tại trên hệ thống!";
        return result;
    }

    std::string role = operatorUser->getRole();

    // 2. Kiểm tra RBAC: Khách hàng phải là chủ sở hữu tài khoản
    if (role == "CUSTOMER") {
        bool isOwner = m_accountRepo.isAccountOwnedByUser(stk, operatorUserId);
        if (!isOwner) {
            result.success = false;
            result.message = "Lỗi phân quyền: Khách hàng chỉ được phép xem sao kê tài khoản chính chủ!";
            return result;
        }
    }

    // 3. Lấy dữ liệu phân trang từ Repository
    result = m_accountRepo.getHistoryPaginated(stk, page, limit);
    return result;
}

// =========================================================================
// TÍNH NĂNG 3: MỞ TÀI KHOẢN MỚI (eKYC KHÁCH HÀNG / GDV MỞ TẠI QUẦY)
// =========================================================================
TransactionResult AccountService::openNewAccount(const std::string& operatorUserId, 
                                                const std::string& targetUserId, 
                                                const std::string& accountType,
                                                const std::string& customerPin,
                                                bool kycVerified,
                                                int64_t initialBalanceVnd) {
    TransactionResult result;

    // 1. Xác thực người dùng thực hiện
    std::shared_ptr<User> operatorUser = m_userRepo.findById(operatorUserId);
    if (!operatorUser) {
        result.success = false;
        result.message = "Không xác thực được người dùng thực hiện thao tác!";
        return result;
    }

    // 2. Kiểm tra RBAC mở tài khoản
    std::string role = operatorUser->getRole();
    if (role == "CUSTOMER") {
        if (operatorUserId != targetUserId) {
            result.success = false;
            result.message = "Lỗi phân quyền: Khách hàng chỉ được phép tự mở tài khoản eKYC cho chính mình!";
            return result;
        }
    } else if (role != "TELLER" && role != "ADMIN") {
        result.success = false;
        result.message = "Người dùng không có quyền mở tài khoản!";
        return result;
    }

    if ((role == "TELLER" || role == "ADMIN") && !kycVerified) {
        result.success = false;
        result.message = "Cần xác nhận đã đối chiếu CCCD bản gốc trước khi mở tài khoản.";
        return result;
    }
    if (role == "TELLER" && accountType != "CHECKING") {
        result.success = false;
        result.message = "Giao dịch viên hiện chỉ được mở tài khoản thanh toán tại quầy.";
        return result;
    }

    if (accountType != "CHECKING" && accountType != "CREDIT" &&
        accountType != "SAVINGS" && accountType != "LOAN") {
        result.success = false;
        result.message = "Loại tài khoản không được hỗ trợ!";
        return result;
    }
    if (initialBalanceVnd < 0 || initialBalanceVnd > 500000000 ||
        (initialBalanceVnd > 0 && accountType != "CHECKING")) {
        result.success = false;
        result.message = "Số tiền nạp ban đầu không hợp lệ hoặc chỉ áp dụng cho tài khoản thanh toán.";
        return result;
    }

    if (targetUserId.empty()) {
        result.success = false;
        result.message = "Thiếu thông tin khách hàng cần mở tài khoản!";
        return result;
    }
    auto targetUser = m_userRepo.findById(targetUserId);
    if (!targetUser || targetUser->getRole() != "CUSTOMER" || !targetUser->isActive()) {
        result.success = false;
        result.message = "Khách hàng không tồn tại hoặc không hoạt động!";
        return result;
    }
    if (role == "CUSTOMER") {
        if (targetUserId != operatorUserId) {
            result.success = false;
            result.message = "Khách hàng chỉ được tự mở tài khoản chính chủ!";
            return result;
        }
    } else if (!(role == "TELLER" && accountType == "CHECKING") && !targetUser->verifyPin(customerPin)) {
        result.success = false;
        result.message = "Mã PIN xác thực của khách hàng không chính xác!";
        return result;
    }

    // 3. Sinh số tài khoản mới & Khởi tạo trạng thái ban đầu
    std::string newStk = generateNewAccountId(accountType);

    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    std::string createdAt(buf);

    // 4. Lưu tài khoản mới xuống CSDL
    bool created = m_accountRepo.createNewAccount(newStk, targetUserId, accountType, initialBalanceVnd, "ACTIVE", createdAt);
    if (!created) {
        result.success = false;
        result.message = "Lỗi CSDL khi tạo tài khoản mới!";
        return result;
    }

    // 5. Ghi nhận Audit Log
    m_auditRepo.logAction(operatorUserId, "OPEN_ACCOUNT_SUCCESS", newStk, 
                          "Type: " + accountType + " for User: " + targetUserId + "; KYC checked; opening deposit: " + std::to_string(initialBalanceVnd));

    result.success = true;
    result.message = "Mở tài khoản " + accountType + " mới thành công!";
    result.transaction_id = newStk;
    result.post_balance_vnd = initialBalanceVnd;
    return result;
}

// =========================================================================
// TÍNH NĂNG 4: KHÓA / MỞ KHÓA TÀI KHOẢN KHẨN CẤP
// =========================================================================
TransactionResult AccountService::toggleAccountLock(const std::string& operatorUserId, 
                                                   const std::string& stk, 
                                                   bool lockRequest, 
                                                   const std::string& pinOrVerifyCode) {
    TransactionResult result;

    // 1. Xác thực người thao tác
    std::shared_ptr<User> operatorUser = m_userRepo.findById(operatorUserId);
    if (!operatorUser) {
        result.success = false;
        result.message = "Người dùng thực hiện không tồn tại!";
        return result;
    }

    std::string role = operatorUser->getRole();

    // 2. Quy tắc an ninh & Phân quyền RBAC
    if (role == "CUSTOMER") {
        // Khách hàng chỉ được tác động tài khoản chính chủ
        if (!m_accountRepo.isAccountOwnedByUser(stk, operatorUserId)) {
            result.success = false;
            result.message = "Lỗi phân quyền: Khách hàng không sở hữu tài khoản này!";
            return result;
        }

        // Khách hàng CHỈ ĐƯỢC TỰ KHÓA. Khi mở khóa phải ra quầy xác minh
        if (!lockRequest) {
            result.success = false;
            result.message = "BẢO MẬT: Khách hàng chỉ được tự KHÓA khẩn cấp. Vui lòng liên hệ Giao dịch viên tại quầy để MỞ KHÓA!";
            return result;
        }

        // Xác thực mã PIN 6 số trước khi khóa
        if (!operatorUser->verifyPin(pinOrVerifyCode)) {
            m_auditRepo.logAction(operatorUserId, "LOCK_ACCOUNT_FAILED", stk, "Mã PIN không chính xác");
            result.success = false;
            result.message = "Mã PIN xác thực khóa khẩn cấp không chính xác!";
            return result;
        }
    } 
    else if (role == "TELLER" || role == "ADMIN") {
        // GDV / Admin mở hoặc khóa sau khi đối chiếu giấy tờ định danh (CCCD)
    }

    // The accounts schema uses BLOCKED for locked accounts.
    std::string newStatus = lockRequest ? "BLOCKED" : "ACTIVE";
    bool updated = m_accountRepo.updateAccountStatus(stk, newStatus);

    if (!updated) {
        result.success = false;
        result.message = "Lỗi CSDL khi cập nhật trạng thái tài khoản!";
        return result;
    }

    // 4. Ghi nhận Audit Log
    std::string logActionStr = lockRequest ? "LOCK_ACCOUNT_SUCCESS" : "UNLOCK_ACCOUNT_SUCCESS";
    m_auditRepo.logAction(operatorUserId, logActionStr, stk, "Operator: " + role);

    result.success = true;
    result.message = lockRequest ? "Đã khóa khẩn cấp tài khoản thành công!" : "Đã mở khóa tài khoản thành công!";
    result.transaction_id = stk;
    return result;
}

} // namespace Banking
