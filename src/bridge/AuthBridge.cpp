#include "bridge/AuthBridge.hpp"
#include "services/AuthService.hpp"
#include "dtos/AuthDTOs.hpp"
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

AuthBridge::AuthBridge(AuthService& service, QObject* parent)
    : QObject(parent), m_service(service) {}

// 1. API Đăng nhập
QString AuthBridge::login(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<LoginRequest>();
        LoginResult res = m_service.login(req);
        json j = res;
        return QString::fromStdString(j.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// 2. API Đăng ký Khách hàng
QString AuthBridge::registerCustomer(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<RegisterRequest>();
        RegisterResult res = m_service.registerCustomer(req);
        json j = res;
        return QString::fromStdString(j.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// 3. API Quên / Đặt lại mật khẩu
QString AuthBridge::forgotPassword(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<ForgotPasswordRequest>();
        ForgotPasswordResult res = m_service.forgotPassword(req);
        json j = res;
        return QString::fromStdString(j.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// 4. API Đăng xuất
bool AuthBridge::logout(const QString& sessionToken) {
    return m_service.logout(sessionToken.toStdString());
}

} // namespace Banking