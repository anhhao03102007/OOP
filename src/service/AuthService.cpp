#include "services/AuthService.hpp"
#include <algorithm>
#include <cctype>

namespace Banking {

namespace {

std::string trim(const std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    return first < last ? std::string(first, last) : std::string{};
}

bool isValidEmail(const std::string& email) {
    const auto at = email.find('@');
    if (at == std::string::npos || at == 0 || at != email.rfind('@') || email.size() > 254) return false;
    const std::string local = email.substr(0, at);
    const std::string domain = email.substr(at + 1);
    if (local.size() > 64 || local.front() == '.' || local.back() == '.' ||
        local.find("..") != std::string::npos || domain.size() < 3 || domain.front() == '.' ||
        domain.back() == '.' || domain.find('.') == std::string::npos) return false;
    const auto validLocal = std::all_of(local.begin(), local.end(), [](unsigned char ch) {
        return std::isalnum(ch) || std::string(".!#$%&'*+/=?^_`{|}~-").find(static_cast<char>(ch)) != std::string::npos;
    });
    if (!validLocal) return false;
    bool labelStart = true;
    bool dotSeen = false;
    for (size_t i = 0; i < domain.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(domain[i]);
        if (ch == '.') {
            if (labelStart || domain[i - 1] == '-') return false;
            labelStart = true;
            dotSeen = true;
        } else if (std::isalnum(ch)) {
            labelStart = false;
        } else if (ch == '-' && !labelStart && i + 1 < domain.size() && domain[i + 1] != '.') {
            continue;
        } else {
            return false;
        }
    }
    return dotSeen && !labelStart && domain.back() != '-';
}

bool isDigits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isdigit(ch);
    });
}

}  // namespace

AuthService::AuthService(AuthRepository& authRepo, UserRepository& userRepo,
                         AuditRepository& auditRepo,
                         SessionManager& sessionManager)
    : m_authRepo(authRepo), m_userRepo(userRepo),
      m_auditRepo(auditRepo), m_sessionManager(sessionManager) {}

// 1. ĐĂNG NHẬP HỆ THỐNG
LoginResult AuthService::login(const LoginRequest& req) {
    LoginResult result;
    const std::string GENERIC_ERROR = "Thông tin đăng nhập không chính xác hoặc tài khoản đang bị khóa!";

    // 1. Tra cứu người dùng qua Username / Phone / Email / CCCD
    auto user = m_authRepo.findByIdentifier(req.identifier);
    if (!user) {
        m_auditRepo.logAction("GUEST", "LOGIN_FAILED", req.identifier, "Identifier not found");
        result.success = false;
        result.message = GENERIC_ERROR;
        return result;
    }

    // Tự mở khóa khi hết thời gian; khóa vĩnh viễn dùng locked_until = 0.
    if (m_authRepo.releaseExpiredTemporaryLock(user->getUserId()))
        user = m_authRepo.findByIdentifier(req.identifier);

    // 2. Kiểm tra tài khoản bị khóa vĩnh viễn hoặc khóa tạm thời
    if (user->isBlocked()) {
        m_auditRepo.logAction(user->getUserId(), "LOGIN_BLOCKED", user->getUserId(), "Account is blocked");
        result.success = false;
        result.message = GENERIC_ERROR;
        return result;
    }

    // 3. Xác thực Mật khẩu qua PBKDF2-HMAC-SHA256
    bool passValid = AuthSecurity::verifyPassword(req.password, user->getPasswordHash(), user->getSalt());

    if (!passValid) {
        // 3a. Tăng số lần đăng nhập sai từ giá trị RAM hiện tại
        int failedCount = user->getFailedAttempts() + 1;
        
        // 3b. Cập nhật số lần sai xuống TursoDB
        m_authRepo.updateFailedAttempts(user->getUserId(), failedCount);
        
        // 3c. Cập nhật lại giá trị trên đối tượng RAM để giữ đồng bộ trong phiên
        user->setFailedAttempts(failedCount);

        // 3d. Ghi vết Audit Log
        m_auditRepo.logAction(user->getUserId(), "LOGIN_FAILED", user->getUserId(),
                              "Sai mật khẩu. Số lần nhập sai: " + std::to_string(failedCount));

        // 3e. Nếu đạt ngưỡng tối đa (5 lần), tự động khóa tạm thời 15 phút
        if (failedCount >= MAX_FAILED_ATTEMPTS) {
            m_authRepo.lockUserTemporarily(user->getUserId(), LOCK_DURATION_MINUTES);
            m_auditRepo.logAction("SYSTEM", "USER_LOCKOUT", user->getUserId(), 
                                  "Khóa tài khoản " + std::to_string(LOCK_DURATION_MINUTES) + " phút do nhập sai 5 lần liên tiếp");
        }

        result.success = false;
        result.message = GENERIC_ERROR;
        return result;
    }

    // 4. Đăng nhập thành công -> Reset đếm sai
    m_authRepo.resetFailedAttempts(user->getUserId());
    if (user->getPasswordHash().rfind("pbkdf2_sha256$", 0) != 0) {
        const std::string upgradedSalt = AuthSecurity::generateSalt();
        m_authRepo.updatePassword(user->getUserId(), AuthSecurity::hashPasswordPBKDF2(req.password, upgradedSalt), upgradedSalt);
    }

    // 5. Lấy tài khoản thanh toán mặc định
    std::string primaryAcc = m_authRepo.getPrimaryAccountId(user->getUserId());
    auto Curbalance = m_authRepo.getBalance(primaryAcc);

    // 6. Tạo phiên cô lập trên RAM (Frontend chỉ nhận session_token)
    std::string token = m_sessionManager.createSession(
        user->getUserId(), user->getFullName(), user->getRole(), primaryAcc
    );

    m_auditRepo.logAction(user->getUserId(), "LOGIN_SUCCESS", user->getUserId(), "Role: " + user->getRole());

    // 7. Trả kết quả DTO
    result.success = true;
    result.message = "Đăng nhập thành công!";
    result.session.session_token = token;
    result.session.user_id = user->getUserId();
    result.session.full_name = user->getFullName();
    result.session.role = user->getRole();

    return result;
}

// 2. ĐĂNG KÝ TÀI KHOẢN KHÁCH HÀNG MỚI
RegisterResult AuthService::registerCustomer(const RegisterRequest& req) {
    RegisterResult result;

    const std::string username = trim(req.username);
    const std::string fullname = trim(req.fullname);
    const std::string email = trim(req.email);
    const std::string cccd = trim(req.cccd);
    const std::string phoneInput = trim(req.phone);
    std::string normalizedPhone;
    for (size_t i = 0; i < phoneInput.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(phoneInput[i]);
        if (std::isdigit(ch)) {
            normalizedPhone += static_cast<char>(ch);
        } else if (ch == '+' && i == 0) {
            continue;
        } else if (ch != ' ' && ch != '-' && ch != '(' && ch != ')') {
            normalizedPhone.clear();
            break;
        }
    }

    if (username.empty() || username.size() > 50 ||
        fullname.empty() || fullname.size() > 150 ||
        req.password.size() < 8 ||
        !isValidEmail(email) ||
        cccd.size() != 12 || !isDigits(cccd) ||
        (normalizedPhone.size() != 10 && normalizedPhone.size() != 11) ||
        !isDigits(normalizedPhone)) {
        result.message = "Vui lòng nhập đầy đủ thông tin hợp lệ; mật khẩu tối thiểu 8 ký tự, CCCD gồm 12 số và SĐT gồm 10-11 số.";
        return result;
    }

    if (m_authRepo.existsByIdentifier(username) ||
        m_authRepo.existsByIdentifier(email) ||
        m_authRepo.existsByIdentifier(normalizedPhone) ||
        m_authRepo.existsByIdentifier(cccd)) {
        result.success = false;
        result.message = "Tên đăng nhập, Email, SĐT hoặc Số CCCD đã tồn tại trên hệ thống!";
        return result;
    }

    std::string salt = AuthSecurity::generateSalt();
    std::string passHash = AuthSecurity::hashPasswordPBKDF2(req.password, salt);

    const std::string newUserId = "CUS_" + AuthSecurity::generateNumericCode(24);
    const std::string newAccId = "ACC_" + AuthSecurity::generateNumericCode(24);

    const bool ok = m_authRepo.createCustomerWithCheckingAccount(
        newUserId, username, passHash, salt, fullname, email, cccd, normalizedPhone, newAccId);

    if (ok) {
        m_auditRepo.logAction("SYSTEM", "REGISTER_SUCCESS", newUserId, "Created customer account: " + newAccId);
        result.success = true;
        result.message = "Đăng ký tài khoản thành công!";
        result.user_id = newUserId;
        result.account_id = newAccId;
    } else {
        result.success = false;
        result.message = "Lỗi CSDL khi khởi tạo tài khoản!";
    }

    return result;
}

// 3. QUÊN MẬT KHẨU / ĐẶT LẠI MẬT KHẨU
ForgotPasswordResult AuthService::forgotPassword(const ForgotPasswordRequest& req) {
    ForgotPasswordResult res;
    const auto user = m_authRepo.findByIdentifier(req.identifier);
    if (!user || user->getRole() != "CUSTOMER" || req.new_password.size() < 8 || req.cccd.size() != 12 ||
        !isDigits(req.cccd) || user->getCCCD() != req.cccd || !user->verifyPin(req.pin)) {
        res.message = "Không xác minh được CCCD/mã PIN hoặc mật khẩu mới không hợp lệ.";
        return res;
    }
    const std::string salt = AuthSecurity::generateSalt();
    if (!m_authRepo.updatePassword(user->getUserId(), AuthSecurity::hashPasswordPBKDF2(req.new_password, salt), salt)) {
        res.message = "Không lưu được mật khẩu mới vào database.";
        return res;
    }
    res.success = true;
    res.message = "Đặt lại mật khẩu thành công. Vui lòng đăng nhập lại.";
    return res;
}

// 4. ĐĂNG XUẤT (THU HỒI PHIÊN RAM)
bool AuthService::logout(const std::string& sessionToken) {
    auto sessOpt = m_sessionManager.getSession(sessionToken);
    if (sessOpt.has_value()) {
        m_auditRepo.logAction(sessOpt->user_id, "LOGOUT", sessOpt->user_id, "User logged out");
        m_sessionManager.revokeSession(sessionToken);
        return true;
    }
    return false;
}

} // namespace Banking
