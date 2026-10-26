#include "bridge/TransactionBridge.hpp"
#include "services/TransactionService.hpp"
#include "dtos/TransactionDTOs.hpp"
#include "core/AuthSecurity.hpp"
#include <core/json.hpp>
#include <algorithm>
#include <cctype>

namespace Banking {

using json = nlohmann::json;

namespace {
std::string counterExecutionId(const json& request, const std::string& fallback) {
    const std::string requested = request.value("execution_id", "");
    if (requested.size() != 29 || requested.rfind("CASH_", 0) != 0 ||
        !std::all_of(requested.begin() + 5, requested.end(), [](unsigned char c) { return std::isdigit(c); })) {
        return fallback;
    }
    return requested;
}
}

TransactionBridge::TransactionBridge(TransactionService& service, QObject* parent)
    : QObject(parent), m_service(service) {}

QString TransactionBridge::requestOtp(const QString& jsonRequest) {
    try {
        const auto request = json::parse(jsonRequest.toStdString()).get<OtpChallengeRequest>();
        return QString::fromStdString(json(m_service.requestOtp(request)).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu OTP không hợp lệ."}}).dump());
    }
}

// =========================================================================
// 1. API CHUYỂN TIỀN (NỘI BỘ & LIÊN NGÂN HÀNG)
// =========================================================================
QString TransactionBridge::transfer(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<TransferRequest>();
        TransactionResult res = m_service.executeTransfer(req);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 2. API RÚT TIỀN MẶT (ATM / TẠI QUẦY)
// =========================================================================
QString TransactionBridge::withdraw(const QString& jsonRequest) {
    try {
        json jReq = json::parse(jsonRequest.toStdString());
        std::string sessionToken = jReq.value("session_token", "");
        std::string pin          = jReq.value("pin", "");
        std::string otp          = jReq.value("otp", "");
        std::string challengeId  = jReq.value("otp_challenge_id", "");
        std::string idempotency  = jReq.value("idempotency_key", "");
        int64_t amountVnd        = jReq.value("amount_vnd", 0LL);
        std::string description  = jReq.value("description", "");
        bool isAtmOffNet         = jReq.value("is_atm_offnet", false);

        TransactionResult res = m_service.executeWithdraw(sessionToken, pin, otp, challengeId, idempotency,
                                                           amountVnd, description, isAtmOffNet);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 3. API THANH TOÁN HÓA ĐƠN (ĐIỆN, NƯỚC, INTERNET...)
// =========================================================================
QString TransactionBridge::payBill(const QString& jsonRequest) {
    try {
        json jReq = json::parse(jsonRequest.toStdString());
        std::string sessionToken = jReq.value("session_token", "");
        std::string pin          = jReq.value("pin", "");
        std::string billId       = jReq.value("bill_id", "");
        std::string description  = jReq.value("description", "");

        TransactionResult res = m_service.executeBillPayment(sessionToken, pin, billId, description);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 4. API NẠP TIỀN MẶT TẠI QUẦY (DÀNH CHO TELLER / ADMIN)
// =========================================================================
QString TransactionBridge::depositCounter(const QString& jsonRequest) {
    std::string executionId = "CASH_" + AuthSecurity::generateNumericCode(24);
    std::string sessionToken;
    std::string targetAccountId;
    int64_t amountVnd = 0;
    try {
        const json jReq = json::parse(jsonRequest.toStdString());
        executionId = counterExecutionId(jReq, executionId);
        sessionToken = jReq.value("session_token", "");
        targetAccountId = jReq.value("target_account_id", "");
        amountVnd = jReq.value("amount_vnd", 0LL);
        const std::string description = jReq.value("description", "");

        const CounterOperationResult result = m_service.executeDeposit(sessionToken, targetAccountId, amountVnd, description, executionId);
        return QString::fromStdString(json(result).dump());
    } catch (...) {
        const bool persisted = m_service.recordCounterExecutionFailure(executionId, sessionToken, "DEPOSIT",
            targetAccountId, amountVnd, "Định dạng JSON hoặc yêu cầu nạp tiền không hợp lệ.");
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"},
                    {"execution_id", executionId}, {"trace_persisted", persisted}};
        return QString::fromStdString(err.dump());
    }
}

QString TransactionBridge::withdrawCounter(const QString& jsonRequest) {
    std::string executionId = "CASH_" + AuthSecurity::generateNumericCode(24);
    std::string sessionToken;
    std::string targetAccountId;
    int64_t amountVnd = 0;
    try {
        const json req = json::parse(jsonRequest.toStdString());
        executionId = counterExecutionId(req, executionId);
        sessionToken = req.value("session_token", "");
        targetAccountId = req.value("target_account_id", "");
        amountVnd = req.value("amount_vnd", 0LL);
        const auto result = m_service.executeCounterWithdraw(sessionToken, targetAccountId,
            req.value("pin", ""), amountVnd, executionId);
        return QString::fromStdString(json(result).dump());
    } catch (...) {
        const bool persisted = m_service.recordCounterExecutionFailure(executionId, sessionToken, "WITHDRAW",
            targetAccountId, amountVnd, "Định dạng JSON hoặc yêu cầu rút tiền không hợp lệ.");
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu rút tiền tại quầy không hợp lệ."},
            {"execution_id", executionId}, {"trace_persisted", persisted}}).dump());
    }
}

QString TransactionBridge::topup(const QString& jsonRequest) {
    try {
        const json req = json::parse(jsonRequest.toStdString());
        const auto result = m_service.executeTopup(req.value("session_token", ""), req.value("pin", ""), req.value("carrier", ""),
                                                   req.value("phone", ""), req.value("amount_vnd", 0LL), req.value("idempotency_key", ""));
        return QString::fromStdString(json(result).dump());
    } catch (...) { return QString::fromStdString(json({{"success",false},{"message","Yêu cầu nạp tiền không hợp lệ."}}).dump()); }
}

} // namespace Banking
