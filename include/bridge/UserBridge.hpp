#ifndef USER_BRIDGE_HPP
#define USER_BRIDGE_HPP

#include <QObject>
#include <QString>

namespace Banking {

class UserService;

class UserBridge : public QObject {
    Q_OBJECT

private:
    UserService& m_userService;

public:
    explicit UserBridge(UserService& userService, QObject* parent = nullptr);

    // 1. API LẤY THÔNG TIN HỒ SƠ NGƯỜI DÙNG (GET PROFILE)
    Q_INVOKABLE QString getProfile(const QString& jsonRequest);

    // 2. API CẬP NHẬT THÔNG TIN HỒ SƠ CÁ NHÂN (UPDATE PROFILE)
    Q_INVOKABLE QString updateProfile(const QString& jsonRequest);

    // 3. API ĐỔI MẬT KHẨU ĐĂNG NHẬP (CHANGE PASSWORD)
    Q_INVOKABLE QString changePassword(const QString& jsonRequest);

    // 4. API ĐỔI MÃ PIN GIAO DỊCH 6 SỐ (CHANGE PIN)
    Q_INVOKABLE QString changePin(const QString& jsonRequest);

    // 5. API TRA CỨU KHÁCH HÀNG (TELLER / ADMIN SEARCH)
    Q_INVOKABLE QString searchCustomers(const QString& jsonRequest);

    // 6. API KHÓA / MỞ KHÓA TÀI KHOẢN KHÁCH HÀNG (TOGGLE USER LOCK)
    Q_INVOKABLE QString toggleUserLock(const QString& jsonRequest);
};

} // namespace Banking

#endif // USER_BRIDGE_HPP
