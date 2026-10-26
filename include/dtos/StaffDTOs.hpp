#ifndef STAFF_DTOS_HPP
#define STAFF_DTOS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

struct ToggleUserStatusRequest {
    std::string session_token;
    std::string target_user_id;
    std::string action; // "LOCK" hoặc "UNLOCK"
    std::string reason;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ToggleUserStatusRequest, session_token, target_user_id, action, reason)
};

struct OpenCounterAccountRequest {
    std::string session_token;
    std::string customer_user_id;
    std::string account_type;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(OpenCounterAccountRequest, session_token, customer_user_id, account_type)
};

struct CreateStaffRequest {
    std::string session_token;
    std::string username;
    std::string fullname;
    std::string email;
    std::string phone;
    std::string cccd;
    std::string role; // "TELLER" hoặc "ADMIN"
    std::string password;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CreateStaffRequest, session_token, username, fullname, email, phone, cccd, role, password)
};

struct StaffMemberDTO {
    std::string user_id;
    std::string username;
    std::string fullname;
    std::string email;
    std::string phone;
    std::string role;
    std::string status;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StaffMemberDTO, user_id, username, fullname, email, phone, role, status)
};

struct StaffMembersResult {
    bool success = false;
    std::string message;
    std::vector<StaffMemberDTO> staff;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StaffMembersResult, success, message, staff)
};

struct StaffOperationResult {
    bool success = false;
    std::string message;
    std::string created_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StaffOperationResult, success, message, created_id)
};

struct LoanTermConfigDTO {
    int months = 0;
    bool enabled = false;
    double rate = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanTermConfigDTO, months, enabled, rate)
};

struct GetSystemConfigRequest {
    std::string session_token;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GetSystemConfigRequest, session_token)
};

struct SystemConfigDTO {
    bool success = false;
    std::string message;
    int64_t interbank_fee_vnd = 9900;
    int64_t atm_fee_vnd = 3300;
    double savings_max_rate_percent = 5.8;
    double credit_monthly_rate_percent = 2.0;
    int credit_due_day = 15;
    double credit_min_payment_percent = 5.0;
    int64_t counter_max_transaction_vnd = 500000000;
    std::vector<LoanTermConfigDTO> loan_terms;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SystemConfigDTO, success, message, interbank_fee_vnd,
                                   atm_fee_vnd, savings_max_rate_percent, loan_terms,
                                   credit_monthly_rate_percent, credit_due_day, credit_min_payment_percent,
                                   counter_max_transaction_vnd)
};

struct UpdateSystemConfigRequest {
    std::string session_token;
    int64_t interbank_fee_vnd = 0;
    int64_t atm_fee_vnd = 0;
    double savings_max_rate_percent = 0.0;
    double credit_monthly_rate_percent = 0.0;
    int credit_due_day = 15;
    double credit_min_payment_percent = 0.0;
    int64_t counter_max_transaction_vnd = 500000000;
    std::vector<LoanTermConfigDTO> loan_terms;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UpdateSystemConfigRequest, session_token, interbank_fee_vnd,
                                   atm_fee_vnd, savings_max_rate_percent, loan_terms,
                                   credit_monthly_rate_percent, credit_due_day, credit_min_payment_percent,
                                   counter_max_transaction_vnd)
};

struct DashboardStatsDTO {
    bool success = false;
    std::string message;
    int64_t total_users = 0;
    int64_t total_customers = 0;
    int64_t total_transactions = 0;
    int64_t total_balance_vnd = 0;
    int64_t pending_loans = 0;
    int64_t locked_users = 0;
    int64_t today_transactions = 0;
    int64_t today_served_customers = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DashboardStatsDTO, success, message, total_users,
        total_customers, total_transactions, total_balance_vnd, pending_loans,
        locked_users, today_transactions, today_served_customers)
};

} // namespace Banking

#endif // STAFF_DTOS_HPP
