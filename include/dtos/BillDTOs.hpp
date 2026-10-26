#ifndef BILL_DTOS_HPP
#define BILL_DTOS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

struct QueryBillRequest {
    std::string session_token;
    std::string provider_id;
    std::string customer_code;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(QueryBillRequest, session_token, provider_id, customer_code)
};

struct BillDTO {
    std::string bill_id;
    std::string provider_id;
    std::string provider_name;
    std::string customer_code;
    std::string customer_name;
    int64_t     amount_vnd = 0;
    std::string service_type;
    std::string status;
    std::string due_date;
    std::string period;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BillDTO, bill_id, provider_id, provider_name, customer_code, customer_name, amount_vnd, service_type, status, due_date, period)
};

struct BillProviderDTO {
    std::string provider_id;
    std::string provider_name;
    std::string service_type;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BillProviderDTO, provider_id, provider_name, service_type)
};

struct GetBillProvidersResult {
    bool success = false;
    std::string message;
    std::vector<BillProviderDTO> providers;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GetBillProvidersResult, success, message, providers)
};

struct QueryBillResult {
    bool success = false;
    std::string message;
    BillDTO bill;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(QueryBillResult, success, message, bill)
};

struct PayBillRequest {
    std::string session_token;
    std::string idempotency_key;
    std::string bill_id;
    std::string pin;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PayBillRequest, session_token, idempotency_key, bill_id, pin)
};

struct PayBillResult {
    bool success = false;
    std::string message;
    std::string transaction_id;
    int64_t post_balance_vnd = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PayBillResult, success, message, transaction_id, post_balance_vnd)
};

} // namespace Banking

#endif // BILL_DTOS_HPP