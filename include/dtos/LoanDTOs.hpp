#ifndef LOAN_DTOS_HPP
#define LOAN_DTOS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

struct ApplyLoanRequest {
    std::string session_token;
    int64_t     amount_vnd = 0;
    int         term_months = 12;
    std::string calc_method = "DECREASING"; // "FLAT" hoặc "DECREASING"
    std::string collateral_info;
    std::string purpose;
    int64_t monthly_income_vnd = 0;
    std::string target_user_id;
    std::string customer_pin;
    bool kyc_verified = false;
    std::vector<std::string> received_document_types;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ApplyLoanRequest, session_token, amount_vnd, term_months, calc_method, collateral_info, purpose, monthly_income_vnd, target_user_id, customer_pin, kyc_verified, received_document_types)
};

struct ApproveLoanRequest {
    std::string session_token;
    std::string loan_id;
    std::string action; // "APPROVE" hoặc "REJECT"
    std::string reason;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ApproveLoanRequest, session_token, loan_id, action, reason)
};

struct DisburseLoanRequest {
    std::string session_token;
    std::string loan_id;
    std::string target_account_id;
    std::string pin;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DisburseLoanRequest, session_token, loan_id, target_account_id, pin)
};

struct LoanDocumentDTO {
    std::string document_id;
    std::string loan_id;
    std::string document_type;
    std::string file_name;
    std::string mime_type;
    int64_t file_size = 0;
    std::string status;
    std::string review_note;
    std::string submitted_at;
    std::string storage_key;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanDocumentDTO, document_id, loan_id, document_type, file_name,
        mime_type, file_size, status, review_note, submitted_at, storage_key)
};

struct LoanDocumentsResult {
    bool success = false;
    std::string message;
    std::vector<LoanDocumentDTO> documents;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanDocumentsResult, success, message, documents)
};

struct LoanDocumentLinkResult {
    bool success = false;
    std::string message;
    std::string url;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanDocumentLinkResult, success, message, url)
};

struct SubmitLoanDocumentRequest {
    std::string session_token;
    std::string loan_id;
    std::string document_type;
    std::string file_name;
    std::string mime_type;
    int64_t file_size = 0;
    std::string content_base64;
    std::string storage_key;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SubmitLoanDocumentRequest, session_token, loan_id, document_type,
        file_name, mime_type, file_size, content_base64, storage_key)
};

struct ReviewLoanDocumentRequest {
    std::string session_token;
    std::string document_id;
    std::string action;
    std::string note;
    std::string loan_id;
    std::string document_name;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ReviewLoanDocumentRequest, session_token, document_id,
        action, note, loan_id, document_name)
};

struct LoanDocumentLinkRequest {
    std::string session_token;
    std::string document_id;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanDocumentLinkRequest, session_token, document_id)
};

struct PayInstallmentRequest {
    std::string session_token;
    std::string schedule_id;
    std::string account_id;
    std::string pin;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PayInstallmentRequest, session_token, schedule_id, account_id, pin)
};

struct LoanScheduleDTO {
    std::string schedule_id;
    std::string loan_id;
    int period_num = 0;
    std::string due_date;
    int64_t principal_due_vnd = 0;
    int64_t interest_due_vnd = 0;
    int64_t penalty_vnd = 0;
    int64_t total_due_vnd = 0;
    std::string status;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanScheduleDTO, schedule_id, loan_id, period_num, due_date,
        principal_due_vnd, interest_due_vnd, penalty_vnd, total_due_vnd, status)
};

struct LoanSchedulesResult {
    bool success = false;
    std::string message;
    std::vector<LoanScheduleDTO> schedules;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanSchedulesResult, success, message, schedules)
};

struct PayInstallmentResult {
    bool success = false;
    std::string message;
    std::string transaction_id;
    std::string schedule_id;
    int64_t amount_vnd = 0;
    int64_t post_balance_vnd = 0;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PayInstallmentResult, success, message, transaction_id,
        schedule_id, amount_vnd, post_balance_vnd)
};

struct LoanDTO {
    std::string loan_id;
    std::string user_id;
    int64_t amount_vnd = 0;
    double interest_rate = 0.0;
    int term_months = 0;
    int64_t monthly_installment_vnd = 0;
    std::string collateral_info;
    std::string purpose;
    int64_t monthly_income_vnd = 0;
    std::string status;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanDTO, loan_id, user_id, amount_vnd, interest_rate, term_months, monthly_installment_vnd, collateral_info, purpose, monthly_income_vnd, status)
};

struct StaffLoanApplicationDTO {
    std::string loan_id;
    std::string user_id;
    std::string customer_name;
    std::string customer_phone;
    int64_t amount_vnd = 0;
    double interest_rate = 0.0;
    int term_months = 0;
    std::string calc_method;
    std::string collateral_info;
    std::string purpose;
    int64_t monthly_income_vnd = 0;
    std::string status;
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StaffLoanApplicationDTO, loan_id, user_id, customer_name, customer_phone, amount_vnd, interest_rate, term_months, calc_method, collateral_info, purpose, monthly_income_vnd, status, created_at)
};

struct StaffLoanApplicationsResult {
    bool success = false;
    std::string message;
    std::vector<StaffLoanApplicationDTO> applications;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StaffLoanApplicationsResult, success, message, applications)
};

struct LoanOperationResult {
    bool success = false;
    std::string message;
    std::string loan_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanOperationResult, success, message, loan_id)
};

} // namespace Banking

#endif // LOAN_DTOS_HPP
