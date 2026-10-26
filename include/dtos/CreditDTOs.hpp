#ifndef CREDIT_DTOS_HPP
#define CREDIT_DTOS_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <core/json.hpp>

namespace Banking {
using json = nlohmann::json;

struct CreditApplicationRequest {
    std::string session_token;
    int64_t requested_limit_vnd = 0;
    int64_t monthly_income_vnd = 0;
    std::string target_user_id;
    std::string customer_pin;
    bool kyc_verified = false;
    std::string file_name;
    std::string mime_type;
    int64_t file_size = 0;
    std::string storage_key;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CreditApplicationRequest, session_token, requested_limit_vnd,
        monthly_income_vnd, target_user_id, customer_pin, kyc_verified, file_name, mime_type, file_size, storage_key)
};
struct CreditDocumentRequest {
    std::string session_token;
    std::string application_id;
    std::string file_name;
    std::string mime_type;
    int64_t file_size = 0;
    std::string storage_key;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CreditDocumentRequest, session_token, application_id,
        file_name, mime_type, file_size, storage_key)
};
struct ReviewCreditDocumentRequest {
    std::string session_token;
    std::string document_id;
    std::string action;
    std::string note;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ReviewCreditDocumentRequest, session_token, document_id,
        action, note)
};
struct CreditDocumentResult {
    bool success = false;
    std::string message;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CreditDocumentResult, success, message)
};
struct CreditDecisionRequest {
    std::string session_token;
    std::string application_id;
    std::string action;
    int64_t approved_limit_vnd = 0;
    std::string note;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CreditDecisionRequest, session_token, application_id, action, approved_limit_vnd, note)
};
struct CreditApplicationsRequest {
    std::string session_token;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CreditApplicationsRequest, session_token)
};
struct CreditApplicationDTO {
    std::string application_id;
    std::string user_id;
    int64_t requested_limit_vnd = 0;
    int64_t monthly_income_vnd = 0;
    std::string status;
    std::string decision_note;
    std::string account_id;
    std::string created_at;
    std::string document_id;
    std::string document_file_name;
    std::string document_mime_type;
    int64_t document_file_size = 0;
    std::string document_storage_key;
    std::string document_status;
    std::string document_review_note;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CreditApplicationDTO, application_id, user_id, requested_limit_vnd,
        monthly_income_vnd, status, decision_note, account_id, created_at, document_id, document_file_name,
        document_mime_type, document_file_size, document_storage_key, document_status, document_review_note)
};
struct CreditApplicationsResult {
    bool success = false;
    std::string message;
    std::string created_application_id;
    std::vector<CreditApplicationDTO> applications;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CreditApplicationsResult, success, message, created_application_id, applications)
};
struct CreditActionRequest {
    std::string session_token;
    std::string idempotency_key;
    std::string account_id;
    std::string receiver_account_id;
    std::string receiver_bank;
    std::string source_account_id;
    int64_t amount_vnd = 0;
    std::string pin;
    std::string description;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CreditActionRequest, session_token, idempotency_key, account_id,
        receiver_account_id, receiver_bank, source_account_id, amount_vnd, pin, description)
};
struct CreditCheckingAccountDTO {
    std::string account_id;
    int64_t balance_vnd = 0;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CreditCheckingAccountDTO, account_id, balance_vnd)
};
struct CreditAccountDTO {
    bool success = false;
    std::string message;
    std::string account_id;
    int64_t limit_vnd = 0;
    int64_t used_vnd = 0;
    int64_t available_vnd = 0;
    double monthly_rate_percent = 0;
    int due_day = 0;
    double minimum_payment_percent = 0;
    int64_t minimum_payment_vnd = 0;
    std::vector<CreditCheckingAccountDTO> checking_accounts;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CreditAccountDTO, success, message, account_id, limit_vnd, used_vnd,
        available_vnd, monthly_rate_percent, due_day, minimum_payment_percent, minimum_payment_vnd, checking_accounts)
};
}
#endif
