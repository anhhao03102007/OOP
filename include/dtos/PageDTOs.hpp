#ifndef PAGE_DTOS_HPP
#define PAGE_DTOS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <core/json.hpp>

// Import các DTOs nền tảng
#include "dtos/UserDTOs.hpp"
#include "dtos/AccountDTOs.hpp"
#include "dtos/TransactionDTOs.hpp"
#include "dtos/LoanDTOs.hpp"

namespace Banking {

using json = nlohmann::json;

// Wrapper phản hồi chung từ C++ Server xuống UI
struct PageDataResponseDTO {
    bool success{false};
    std::string page;
    std::string message;
    json data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PageDataResponseDTO, success, page, message, data)
};

// =========================================================================
// A. DTOs VAI TRÒ KHÁCH HÀNG (CUSTOMER)
// =========================================================================

// 1. Home Customer: Tên, Số tài khoản, Số dư
struct CustomerHomePageDTO {
    std::string full_name;
    std::string account_number;
    int64_t balance{0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CustomerHomePageDTO, full_name, account_number, balance)
};

// 2. Chuyển tiền + Nạp tiền + Thanh toán: Tài khoản thanh toán, Số dư
struct TransactionPaymentPageDTO {
    std::string payment_account_number;
    int64_t balance{0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransactionPaymentPageDTO, payment_account_number, balance)
};

// 3. Lịch sử giao dịch: Thông tin giao dịch cần thiết
struct TransactionsPageDTO {
    std::vector<TransactionHistoryResult> transactions;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransactionsPageDTO, transactions)
};

// 4. Khoản vay (Gồm 5 phần)
struct LoanRegistrationDTO {
    int64_t amount{0};
    int term_months{0};
    std::string calc_method;
    std::string collateral_info;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanRegistrationDTO, amount, term_months, calc_method, collateral_info)
};

struct LoanDocsSupplementDTO {
    std::string cccd_number;
    std::string bank_statement_file;
    std::string collateral_doc_file;
    std::string residence_cert_file;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanDocsSupplementDTO, cccd_number, bank_statement_file, collateral_doc_file, residence_cert_file)
};

struct LoanProcessingDTO {
    std::string loan_id;
    int64_t loan_amount{0};
    int term_months{0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanProcessingDTO, loan_id, loan_amount, term_months)
};

struct LoanDisbursementDTO {
    std::string loan_id;
    int64_t loan_amount{0};
    int term_months{0};
    double interest_rate{0.0};
    int64_t monthly_interest_pay{0};
    int64_t disbursement_fee{0};
    std::string receiving_account_number;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanDisbursementDTO, loan_id, loan_amount, term_months, interest_rate, monthly_interest_pay, disbursement_fee, receiving_account_number)
};

struct LoanRepaymentHistoryDTO {
    std::string timestamp;
    int64_t amount{0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanRepaymentHistoryDTO, timestamp, amount)
};

struct LoanProcessingUnitDTO {
    std::string loan_id;
    int64_t amount{0};
    int term_months{0};
    std::string loan_reason;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanProcessingUnitDTO, loan_id, amount, term_months, loan_reason)
};

struct LoanManagementSubDTO {
    std::string account_number;
    int64_t balance{0};
    std::vector<LoanProcessingUnitDTO> processing_units;
    std::vector<LoanRepaymentHistoryDTO> repayment_history;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoanManagementSubDTO, account_number, balance, processing_units, repayment_history)
};

struct CustomerLoanHubDTO {
    LoanRegistrationDTO registration;
    LoanDocsSupplementDTO docs_supplement;
    LoanProcessingDTO processing;
    LoanDisbursementDTO disbursement;
    LoanManagementSubDTO management;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CustomerLoanHubDTO, registration, docs_supplement, processing, disbursement, management)
};

// =========================================================================
// B. DTOs VAI TRÒ QUẢN TRỊ VIÊN (ADMIN)
// =========================================================================

// Admin Home: Tổng người dùng, Số giao dịch hệ thống, Khoản vay chờ duyệt, Tài khoản đang khóa
struct AdminHomePageDTO {
    int total_users{0};
    int total_system_transactions{0};
    int pending_loans_count{0};
    int locked_accounts_count{0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AdminHomePageDTO, total_users, total_system_transactions, pending_loans_count, locked_accounts_count)
};

// =========================================================================
// C. DTOs VAI TRÒ GIAO DỊCH VIÊN (TELLER)
// =========================================================================

// Teller Home: Tổng giao dịch trong ngày, Số hồ sơ chờ duyệt, Tài khoản đang khóa, Tổng số khách phục vụ
struct TellerHomePageDTO {
    int today_transactions_count{0};
    int pending_applications_count{0};
    int locked_accounts_count{0};
    int today_served_customers_count{0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TellerHomePageDTO, today_transactions_count, pending_applications_count, locked_accounts_count, today_served_customers_count)
};

// Duyệt vay (Giao dịch viên & Admin dùng chung)
struct StaffLoanApprovalPageDTO {
    std::vector<LoanApplicationDTO> pending_loans;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StaffLoanApprovalPageDTO, pending_loans)
};

} // namespace Banking

#endif // PAGE_DTOS_HPP
