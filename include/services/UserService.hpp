#ifndef USER_SERVICE_HPP
#define USER_SERVICE_HPP

#include <string>
#include <vector>
#include <memory>

#include "dtos/UserDTOs.hpp"
#include "repository/UserRepository.hpp"
#include "repository/AuditRepository.hpp"
#include "core/SessionManager.hpp"
#include "core/AuthSecurity.hpp"

namespace Banking {

class UserService {
private:
    UserRepository&  m_userRepo;
    AuditRepository& m_auditRepo;
    SessionManager&  m_sessionManager;

public:
    UserService(UserRepository& userRepo, AuditRepository& auditRepo, SessionManager& sessionManager);

    // 1. LẤY THÔNG TIN HỒ SƠ NGƯỜI DÙNG (PROFILE)
    UserProfileDTO getProfile(const std::string& sessionToken);

    // 2. CẬP NHẬT THÔNG TIN HỒ SƠ CÁ NHÂN
    UserOperationResult updateProfile(const UpdateProfileRequest& req);

    // 3. ĐỔI MẬT KHẨU ĐĂNG NHẬP
    UserOperationResult changePassword(const ChangePasswordRequest& req);

    // 4. ĐỔI MÃ PIN GIAO DỊCH (6 SỐ)
    UserOperationResult changePin(const ChangePinRequest& req);

    // 5. TRA CỨU KHÁCH HÀNG (Dành cho Teller / Admin)
    std::vector<UserProfileDTO> searchCustomers(const CustomerSearchRequest& req);

    // 6. KHÓA / MỞ KHÓA TÀI KHOẢN KHÁCH HÀNG (Teller / Admin)
    UserOperationResult toggleUserLock(const std::string& sessionToken, const std::string& targetUserId);
};

} // namespace Banking

#endif // USER_SERVICE_HPP