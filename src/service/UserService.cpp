#include "services/UserService.hpp"
#include <algorithm>
#include <cctype>

namespace Banking {
namespace {

bool validEmail(const std::string& email) {
    const auto at = email.find('@');
    if (at == std::string::npos || at == 0 || at != email.rfind('@') || email.size() > 254) return false;
    const std::string local = email.substr(0, at);
    const std::string domain = email.substr(at + 1);
    if (local.size() > 64 || local.front() == '.' || local.back() == '.' ||
        local.find("..") != std::string::npos || domain.size() < 3 || domain.front() == '.' ||
        domain.back() == '.' || domain.find('.') == std::string::npos) return false;
    if (!std::all_of(local.begin(), local.end(), [](unsigned char ch) {
        return std::isalnum(ch) || std::string(".!#$%&'*+/=?^_`{|}~-").find(static_cast<char>(ch)) != std::string::npos;
    })) return false;
    bool labelStart = true;
    for (size_t i = 0; i < domain.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(domain[i]);
        if (ch == '.') {
            if (labelStart || domain[i - 1] == '-') return false;
            labelStart = true;
        } else if (std::isalnum(ch)) {
            labelStart = false;
        } else if (ch == '-' && !labelStart && i + 1 < domain.size() && domain[i + 1] != '.') {
            continue;
        } else {
            return false;
        }
    }
    return !labelStart && domain.back() != '-';
}

bool validPhone(const std::string& phone) {
    return (phone.size() == 10 || phone.size() == 11) &&
        std::all_of(phone.begin(), phone.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

} // namespace

UserService::UserService(UserRepository& userRepo, AuditRepository& auditRepo, SessionManager& sessionManager)
    : m_userRepo(userRepo), m_auditRepo(auditRepo), m_sessionManager(sessionManager) {}

// 1. LẤY THÔNG TIN HỒ SƠ NGƯỜI DÙNG (PROFILE)
UserProfileDTO UserService::getProfile(const std::string& sessionToken) {
    UserProfileDTO dto;
    auto sessOpt = m_sessionManager.getSession(sessionToken);
    if (!sessOpt.has_value()) {
        return dto; // Trả về DTO rỗng nếu hết phiên
    }

    auto user = m_userRepo.findById(sessOpt->user_id);
    if (!user) return dto;

    dto.user_id            = user->getUserId();
    dto.username           = user->getUsername();
    dto.fullname           = user->getFullName();
    dto.email              = user->getEmail();
    dto.phone              = user->getPhone();
    dto.role               = user->getRole();
    dto.status             = user->isActive() ? "ACTIVE" : "BLOCKED";
    dto.primary_account_id = sessOpt->primary_account_id;

    return dto;
}

// 2. CẬP NHẬT THÔNG TIN HỒ SƠ CÁ NHÂN
UserOperationResult UserService::updateProfile(const UpdateProfileRequest& req) {
    UserOperationResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }

    if (!validEmail(req.email) || !validPhone(req.phone)) {
        res.message = "Email không đúng định dạng hoặc số điện thoại phải gồm 10–11 chữ số.";
        return res;
    }

    bool ok = m_userRepo.updateProfile(sessOpt->user_id, req.fullname, req.email, req.phone);
    if (ok) {
        res.success = true;
        res.message = "Cập nhật thông tin cá nhân thành công!";
        m_auditRepo.logAction(sessOpt->user_id, "UPDATE_PROFILE", sessOpt->user_id, "Updated name/email/phone");
    } else {
        res.message = "Lỗi CSDL khi cập nhật thông tin cá nhân!";
    }
    return res;
}

// 3. ĐỔI MẬT KHẨU ĐĂNG NHẬP
UserOperationResult UserService::changePassword(const ChangePasswordRequest& req) {
    UserOperationResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }

    auto user = m_userRepo.findById(sessOpt->user_id);
    if (!user) {
        res.message = "Người dùng không tồn tại!";
        return res;
    }

    // Xác thực mật khẩu cũ
    std::string oldHash = AuthSecurity::hashPasswordPBKDF2(req.old_password, user->getSalt());
    if (oldHash != user->getPasswordHash()) {
        res.message = "Mật khẩu cũ không chính xác!";
        return res;
    }

    // Sinh Salt mới và băm mật khẩu mới
    std::string newSalt = AuthSecurity::generateToken(16);
    std::string newHash = AuthSecurity::hashPasswordPBKDF2(req.new_password, newSalt);

    if (m_userRepo.updatePassword(user->getUserId(), newHash, newSalt)) {
        res.success = true;
        res.message = "Đổi mật khẩu thành công!";
        m_auditRepo.logAction(user->getUserId(), "CHANGE_PASSWORD", user->getUserId(), "Password changed successfully");
    } else {
        res.message = "Lỗi CSDL khi đổi mật khẩu!";
    }
    return res;
}

// 4. ĐỔI MÃ PIN GIAO DỊCH (6 SỐ)
UserOperationResult UserService::changePin(const ChangePinRequest& req) {
    UserOperationResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }

    if (req.new_pin.length() != 6) {
        res.message = "Mã PIN mới phải gồm đúng 6 chữ số!";
        return res;
    }

    std::string currentPinHash = m_userRepo.getPinHash(sessOpt->user_id);
    std::string oldPinHash = AuthSecurity::hashPin(req.old_pin);

    if (!currentPinHash.empty() && currentPinHash != oldPinHash) {
        res.message = "Mã PIN hiện tại không chính xác!";
        return res;
    }

    std::string newPinHash = AuthSecurity::hashPin(req.new_pin);
    if (m_userRepo.updatePin(sessOpt->user_id, newPinHash)) {
        res.success = true;
        res.message = "Đổi mã PIN giao dịch thành công!";
        m_auditRepo.logAction(sessOpt->user_id, "CHANGE_PIN", sessOpt->user_id, "PIN changed successfully");
    } else {
        res.message = "Lỗi CSDL khi cập nhật mã PIN!";
    }
    return res;
}

// 5. TRA CỨU KHÁCH HÀNG (Dành cho Teller / Admin)
std::vector<UserProfileDTO> UserService::searchCustomers(const CustomerSearchRequest& req) {
    std::vector<UserProfileDTO> list;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) return list;

    // Kiểm tra quyền Teller/Admin
    auto caller = m_userRepo.findById(sessOpt->user_id);
    if (!caller || (!caller->hasPermission(Permission::TELLER_SEARCH_CUSTOMER))) {
        return list;
    }

    auto users = m_userRepo.searchCustomers(req.query);
    for (const auto& u : users) {
        UserProfileDTO dto;
        dto.user_id  = u->getUserId();
        dto.username = u->getUsername();
        dto.fullname = u->getFullName();
        dto.email    = u->getEmail();
        dto.phone    = u->getPhone();
        dto.role     = u->getRole();
        dto.status   = u->isActive() ? "ACTIVE" : "BLOCKED";
        list.push_back(dto);
    }
    return list;
}

// 6. KHÓA / MỞ KHÓA TÀI KHOẢN KHÁCH HÀNG (Teller / Admin)
UserOperationResult UserService::toggleUserLock(const std::string& sessionToken, const std::string& targetUserId) {
    UserOperationResult res;
    auto sessOpt = m_sessionManager.getSession(sessionToken);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }

    auto operatorUser = m_userRepo.findById(sessOpt->user_id);
    if (!operatorUser || !operatorUser->hasPermission(Permission::TELLER_TOGGLE_USER_LOCK)) {
        res.message = "BẢO MẬT: Bạn không có quyền khóa/mở khóa tài khoản!";
        return res;
    }

    auto targetUser = m_userRepo.findById(targetUserId);
    if (!targetUser) {
        res.message = "Tài khoản mục tiêu không tồn tại!";
        return res;
    }

    // Quy tắc an ninh: Kiểm tra targetUser có được phép bị khóa hay không (Admin return false)
    if (!targetUser->canBeBlocked()) {
        res.message = "BẢO MẬT: Tài khoản Quản trị viên (Admin) không thể bị khóa!";
        return res;
    }

    std::string newStatus = targetUser->isActive() ? "BLOCKED" : "ACTIVE";
    if (m_userRepo.updateStatus(targetUserId, newStatus)) {
        if (newStatus == "BLOCKED") {
            m_sessionManager.revokeAllSessionsForUser(targetUserId);
        }
        res.success = true;
        res.message = (newStatus == "BLOCKED") ? "Đã khóa tài khoản thành công!" : "Đã mở khóa tài khoản thành công!";
        m_auditRepo.logAction(sessOpt->user_id, "TOGGLE_USER_LOCK", targetUserId, "New status: " + newStatus);
    } else {
        res.message = "Lỗi CSDL khi thay đổi trạng thái tài khoản!";
    }
    return res;
}

} // namespace Banking
