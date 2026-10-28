#ifndef TRANSACTION_DTOS_HPP
#define TRANSACTION_DTOS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

struct LookupReceiverRequest {
    std::string session_token;
    std::string receiver_account_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LookupReceiverRequest, session_token, receiver_account_id)
};

struct LookupReceiverResult {
    bool success = false;
    std::string message;
    std::string receiver_name;
    std::string bank_name = "OOPBANK";

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LookupReceiverResult, success, message, receiver_name, bank_name)
};

struct TransferRequest {
    std::string session_token;
    std::string idempotency_key;
    std::string receiver_account_id;
    std::string receiver_bank;
    int64_t     amount_vnd = 0;
    std::string description;
    std::string pin;
    std::string otp;
    std::string otp_challenge_id;
    bool        is_interbank = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransferRequest, session_token, idempotency_key, receiver_account_id, receiver_bank, amount_vnd, description, pin, otp, otp_challenge_id, is_interbank)
};

struct OtpChallengeRequest {
    std::string session_token;
    std::string purpose;
    std::string idempotency_key;
    std::string receiver_account_id;
    int64_t amount_vnd = 0;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(OtpChallengeRequest, session_token, purpose, idempotency_key, receiver_account_id, amount_vnd)
};

struct OtpChallengeResult {
    bool success = false;
    std::string message;
    std::string challenge_id;
    std::string masked_destination;
    std::string otp_code;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(OtpChallengeResult, success, message, challenge_id, masked_destination, otp_code)
};

struct CounterCashRequest {
    std::string session_token;
    std::string account_id;
    int64_t     amount_vnd = 0;
    std::string type; // "DEPOSIT" hoặc "WITHDRAW"
    std::string description;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CounterCashRequest, session_token, account_id, amount_vnd, type, description)
};

struct TransactionResult {
    bool success = false;
    std::string message;
    std::string transaction_id;
    int64_t post_balance_vnd = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransactionResult, success, message, transaction_id, post_balance_vnd)
};

// Kết quả nghiệp vụ tại quầy có ID riêng để tra cứu kể cả khi giao dịch thất bại.
struct CounterOperationResult {
    bool success = false;
    std::string message;
    std::string transaction_id;
    int64_t post_balance_vnd = 0;
    std::string execution_id;
    bool trace_persisted = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CounterOperationResult, success, message, transaction_id, post_balance_vnd, execution_id, trace_persisted)
};

struct TransactionHistoryRequest {
    std::string session_token;
    int page = 1;
    int page_size = 10;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransactionHistoryRequest, session_token, page, page_size)
};

struct TransactionItemDTO {
    std::string transaction_id;
    std::string sender_account;
    std::string receiver_account;
    int64_t amount_vnd = 0;
    int64_t fee_vnd = 0;
    std::string type;
    std::string status;
    std::string description;
    int64_t post_balance_vnd = 0;
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransactionItemDTO, transaction_id, sender_account, receiver_account, amount_vnd, fee_vnd, type, status, description, post_balance_vnd, created_at)
};

struct TransactionHistoryResult {
    bool success = false;
    std::string message;
    int total_items = 0;
    int current_page = 1;
    int total_pages = 1;
    std::vector<TransactionItemDTO> items;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransactionHistoryResult, success, message, total_items, current_page, total_pages, items)
};

} // namespace Banking

#endif // TRANSACTION_DTOS_HPP
