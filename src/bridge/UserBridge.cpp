#include "bridge/UserBridge.hpp"
#include "services/UserService.hpp"
#include "dtos/UserDTOs.hpp"
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

UserBridge::UserBridge(UserService& userService, QObject* parent)
    : QObject(parent), m_userService(userService) {}

// =========================================================================
// 1. API LẤY THÔNG TIN HỒ SƠ NGƯỜI DÙNG (GET PROFILE)
// =========================================================================
QString UserBridge::getProfile(const QString& jsonRequest) {
    try {
        json jReq = json::parse(jsonRequest.toStdString());
        std::string sessionToken = jReq.value("session_token", "");

        UserProfileDTO profile = m_userService.getProfile(sessionToken);
        if (profile.user_id.empty()) {
            json err = {{"success", false}, {"message", "Phiên làm việc hết hạn hoặc không tìm thấy người dùng!"}};
            return QString::fromStdString(err.dump());
        }

        json jRes = profile;
        jRes["success"] = true;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 2. API CẬP NHẬT THÔNG TIN HỒ SƠ CÁ NHÂN (UPDATE PROFILE)
// =========================================================================
QString UserBridge::updateProfile(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<UpdateProfileRequest>();
        UserOperationResult res = m_userService.updateProfile(req);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 3. API ĐỔI MẬT KHẨU ĐĂNG NHẬP (CHANGE PASSWORD)
// =========================================================================
QString UserBridge::changePassword(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<ChangePasswordRequest>();
        UserOperationResult res = m_userService.changePassword(req);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 4. API ĐỔI MÃ PIN GIAO DỊCH 6 SỐ (CHANGE PIN)
// =========================================================================
QString UserBridge::changePin(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<ChangePinRequest>();
        UserOperationResult res = m_userService.changePin(req);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 5. API TRA CỨU KHÁCH HÀNG (TELLER / ADMIN SEARCH)
// =========================================================================
QString UserBridge::searchCustomers(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<CustomerSearchRequest>();
        std::vector<UserProfileDTO> list = m_userService.searchCustomers(req);

        json jRes = {
            {"success", true},
            {"customers", list}
        };
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 6. API KHÓA / MỞ KHÓA TÀI KHOẢN KHÁCH HÀNG (TOGGLE USER LOCK)
// =========================================================================
QString UserBridge::toggleUserLock(const QString& jsonRequest) {
    try {
        json jReq = json::parse(jsonRequest.toStdString());
        std::string sessionToken = jReq.value("session_token", "");
        std::string targetUserId = jReq.value("target_user_id", "");

        UserOperationResult res = m_userService.toggleUserLock(sessionToken, targetUserId);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

} // namespace Banking