#ifndef AUTH_DTOS_HPP
#define AUTH_DTOS_HPP

#include <string>
#include <cstdint>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

// 1. Đăng nhập
struct LoginRequest {
    std::string identifier; // Username / Email / Phone / CCCD
    std::string password;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoginRequest, identifier, password)
};

struct UserSessionDTO {
    std::string session_token;
    std::string user_id;
    std::string full_name;
    std::string role;
    int64_t     created_at = 0;
    int64_t     expires_at = 0;
    std::string primary_account_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UserSessionDTO, session_token, user_id, full_name, role, created_at, expires_at,primary_account_id)
};

struct CustomerDashboardData {
    std::string primary_account_id;
    double balance = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CustomerDashboardData, primary_account_id, balance)
};

struct TellerDashboardData {
    int today_transactions = 0;
    int pending_loans = 0;
    int locked_accounts = 0;
    int today_customers_served = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TellerDashboardData, today_transactions, pending_loans, locked_accounts, today_customers_served)
};

struct AdminDashboardData {
    int total_users = 0;
    double system_total_balance = 0.0;
    int pending_loans = 0;
    int locked_accounts = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AdminDashboardData, total_users, system_total_balance, pending_loans, locked_accounts)
};

struct LoginResult {
    bool           success = false;
    std::string    message;
    UserSessionDTO session;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoginResult, success, message, session)
};

// 2. Đăng ký tài khoản
struct RegisterRequest {
    std::string username;
    std::string password;
    std::string fullname;
    std::string email;
    std::string cccd;
    std::string phone;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RegisterRequest, username, password, fullname, email, cccd, phone)
};

struct RegisterResult {
    bool        success = false;
    std::string message;
    std::string user_id;
    std::string account_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RegisterResult, success, message, user_id, account_id)
};

// 3. Quên / Đặt lại mật khẩu: xác minh CCCD và mã PIN hiện có.
struct ForgotPasswordRequest {
    std::string identifier;
    std::string cccd;
    std::string pin;
    std::string new_password; // Mật khẩu mới

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ForgotPasswordRequest, identifier, cccd, pin, new_password)
};

struct ForgotPasswordResult {
    bool        success = false;
    std::string message;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ForgotPasswordResult, success, message)
};

} // namespace Banking

#endif // AUTH_DTOS_HPP
