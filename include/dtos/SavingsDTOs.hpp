#ifndef SAVINGS_DTOS_HPP
#define SAVINGS_DTOS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

// =========================================================================
// 1. DTO MỞ SỔ TIẾT KIỆM (OPEN SAVINGS)
// =========================================================================
struct OpenSavingsRequest {
    std::string session_token;      // Token xác thực phiên đăng nhập
    std::string idempotency_key;    // Khóa chống gửi trùng lệnh giao dịch
    int64_t     amount_vnd = 0;     // Số tiền gửi gốc (Tối thiểu 1.000.000 VNĐ)
    int         term_months = 6;    // Kỳ hạn (1, 3, 6, 12, 24 tháng)
    std::string maturity_option;    // "RENEW_ALL", "RENEW_PRINCIPAL", "CLOSE_TO_ACCOUNT"
    std::string pin;                // Mã PIN 6 số xác thực giao dịch
    std::string target_user_id;
    std::string source_account_id;
    std::string funding_method = "ACCOUNT"; // ACCOUNT hoặc COUNTER_CASH
    bool        kyc_verified = false; // Nhân viên xác nhận đã đối chiếu CCCD bản gốc

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(OpenSavingsRequest, session_token, idempotency_key, amount_vnd, term_months, maturity_option, pin, target_user_id, source_account_id, funding_method, kyc_verified)
};

// =========================================================================
// 2. DTO TẤT TOÁN SỔ TIẾT KIỆM (CLOSE SAVINGS)
// =========================================================================
struct CloseSavingsRequest {
    std::string session_token;      // Token xác thực phiên
    std::string savings_id;         // Mã sổ tiết kiệm cần tất toán
    std::string pin;                // Mã PIN 6 số xác thực

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CloseSavingsRequest, session_token, savings_id, pin)
};

// =========================================================================
// 3. DTO TRA CỨU DANH SÁCH SỔ TIẾT KIỆM (QUERY SAVINGS LIST)
// =========================================================================
struct GetSavingsListRequest {
    std::string session_token;      // Token người yêu cầu
    std::string target_user_id;     // Mã KH cần tra cứu (để trống nếu tự tra cứu)
    std::string status_filter;      // "ACTIVE", "CLOSED", hoặc "ALL"

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GetSavingsListRequest, session_token, target_user_id, status_filter)
};

// =========================================================================
// 4. DTO THÔNG TIN CHI TIẾT SỔ TIẾT KIỆM (SAVINGS ITEM DTO)
// =========================================================================
struct SavingsDTO {
    std::string savings_id;              // Mã sổ tiết kiệm
    std::string user_id;                 // Mã chủ sở hữu
    std::string source_account_id;       // Tài khoản thanh toán nguồn/thụ hưởng
    int64_t     principal_vnd = 0;       // Số tiền gốc gửi (VNĐ)
    double      interest_rate = 0.0;     // Lãi suất %/năm (VD: 5.8)
    int         term_months = 0;         // Kỳ hạn gửi (tháng)
    int64_t     expected_interest_vnd = 0;// Lãi dự kiến thu được khi đúng hạn
    std::string maturity_option;         // Phương thức đáo hạn
    std::string status;                  // "ACTIVE", "CLOSED", "EARLY_WITHDRAWN"
    std::string start_date;              // Ngày mở sổ (ISO8601)
    std::string maturity_date;           // Ngày đáo hạn (ISO8601)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SavingsDTO, savings_id, user_id, source_account_id, principal_vnd, interest_rate, term_months, expected_interest_vnd, maturity_option, status, start_date, maturity_date)
};

// =========================================================================
// 5. DTO KẾT QUẢ DANH SÁCH SỔ TIẾT KIỆM (SAVINGS LIST RESULT)
// =========================================================================
struct GetSavingsListResult {
    bool success = false;
    std::string message;
    std::vector<SavingsDTO> items;
    int64_t total_principal_vnd = 0;         // Tổng tiền gốc đang gửi
    int64_t total_expected_interest_vnd = 0; // Tổng lãi dự kiến đáo hạn

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GetSavingsListResult, success, message, items, total_principal_vnd, total_expected_interest_vnd)
};

// =========================================================================
// 6. DTO KẾT QUẢ THAO TÁC SỔ TIẾT KIỆM (SAVINGS OPERATION RESULT)
// =========================================================================
struct SavingsOperationResult {
    bool success = false;
    std::string message;
    std::string savings_id;
    int64_t     payout_total_vnd = 0;    // Tổng số tiền nhận về (Gốc + Lãi)
    int64_t     interest_earned_vnd = 0; // Số tiền lãi thực nhận

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SavingsOperationResult, success, message, savings_id, payout_total_vnd, interest_earned_vnd)
};

} // namespace Banking

#endif // SAVINGS_DTOS_HPP
