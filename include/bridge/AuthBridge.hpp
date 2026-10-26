#ifndef AUTH_BRIDGE_HPP
#define AUTH_BRIDGE_HPP

#include <QObject>
#include <QString>

namespace Banking {

class AuthService;

class AuthBridge : public QObject {
    Q_OBJECT

private:
    AuthService& m_service;

public:
    explicit AuthBridge(AuthService& service, QObject* parent = nullptr);

    // 1. API Đăng nhập
    Q_INVOKABLE QString login(const QString& jsonRequest);

    // 2. API Đăng ký Khách hàng
    Q_INVOKABLE QString registerCustomer(const QString& jsonRequest);

    // 3. API Quên / Đặt lại mật khẩu
    Q_INVOKABLE QString forgotPassword(const QString& jsonRequest);

    // 4. API Đăng xuất
    Q_INVOKABLE bool logout(const QString& sessionToken);
};

} // namespace Banking

#endif // AUTH_BRIDGE_HPP