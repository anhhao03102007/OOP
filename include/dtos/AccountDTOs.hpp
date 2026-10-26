#ifndef ACCOUNT_DTOS_HPP
#define ACCOUNT_DTOS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

// DTO biểu diễn thông tin tài khoản
struct AccountDTO {
    std::string account_id;
    std::string user_id;
    int64_t     balance_vnd = 0;
    std::string account_type; // "CHECKING", "SAVINGS"
    std::string status;       // "ACTIVE", "BLOCKED", "CLOSED"
    double      interest_rate = 0.0; // Lãi suất (%/năm nếu là tài khoản tiết kiệm)
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AccountDTO, account_id, user_id, balance_vnd, account_type, status, interest_rate, created_at)
};

// Kết quả lấy danh sách tài khoản
struct GetAccountsResult {
    bool success = false;
    std::string message;
    std::vector<AccountDTO> accounts;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GetAccountsResult, success, message, accounts)
};

// Yêu cầu Khóa / Mở khóa tài khoản khẩn cấp
struct ToggleLockAccountRequest {
    std::string session_token;
    std::string account_id;
    std::string pin;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ToggleLockAccountRequest, session_token, account_id, pin)
};

// Kết quả thao tác tài khoản chung
struct AccountOperationResult {
    bool success = false;
    std::string message;
    std::string account_id;
    int64_t     new_balance_vnd = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AccountOperationResult, success, message, account_id, new_balance_vnd)
};

} // namespace Banking

#endif // ACCOUNT_DTOS_HPP
