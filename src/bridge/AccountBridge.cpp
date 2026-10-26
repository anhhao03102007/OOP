#include "bridge/AccountBridge.hpp"
#include <core/json.hpp>
#include "dtos/AccountDTOs.hpp"
#include "dtos/TransactionDTOs.hpp"

namespace Banking {

using json = nlohmann::json;

AccountBridge::AccountBridge(AccountService& accountService, 
                             SessionManager& sessionManager, 
                             QObject* parent)
    : QObject(parent), 
      m_accountService(accountService), 
      m_sessionManager(sessionManager) {}

// =========================================================================
// 1. API LẤY DANH SÁCH TÀI KHOẢN (TRUY VẤN SỐ DƯ)
// =========================================================================
QString AccountBridge::getAccounts(const QString& jsonRequest) {
    try {
        json jReq = json::parse(jsonRequest.toStdString());
        std::string token = jReq.value("session_token", "");

        auto sessOpt = m_sessionManager.getSession(token);
        if (!sessOpt.has_value()) {
            json err = {{"success", false}, {"message", "Phiên đăng nhập đã hết hạn hoặc không hợp lệ!"}};
            return QString::fromStdString(err.dump());
        }

        // Nếu target_user_id rỗng, mặc định lấy tài khoản của chính người dùng đang đăng nhập
        std::string targetUserId = jReq.value("target_user_id", "");
        if (targetUserId.empty()) {
            targetUserId = sessOpt->user_id;
        }

        GetAccountsResult res = m_accountService.getAccounts(sessOpt->user_id, targetUserId);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 2. API TRUY VẤN LỊCH SỬ GIAO DỊCH (SAO KÊ TÀI KHOẢN)
// =========================================================================
QString AccountBridge::getTransactionHistory(const QString& jsonRequest) {
    try {
        json jReq = json::parse(jsonRequest.toStdString());
        std::string token = jReq.value("session_token", "");

        auto sessOpt = m_sessionManager.getSession(token);
        if (!sessOpt.has_value()) {
            json err = {{"success", false}, {"message", "Phiên đăng nhập đã hết hạn hoặc không hợp lệ!"}};
            return QString::fromStdString(err.dump());
        }

        std::string accountId = jReq.value("account_id", "");
        int limit = jReq.value("limit", 50);
        int page = jReq.value("page", 1);

        TransactionHistoryResult res = m_accountService.getTransactionHistory(sessOpt->user_id, accountId, limit, page);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 3. API MỞ TÀI KHOẢN MỚI (CHECKING / SAVINGS / CREDIT / LOAN)
// =========================================================================
QString AccountBridge::openNewAccount(const QString& jsonRequest) {
    try {
        json jReq = json::parse(jsonRequest.toStdString());
        std::string token = jReq.value("session_token", "");

        auto sessOpt = m_sessionManager.getSession(token);
        if (!sessOpt.has_value()) {
            json err = {{"success", false}, {"message", "Phiên đăng nhập đã hết hạn hoặc không hợp lệ!"}};
            return QString::fromStdString(err.dump());
        }

        std::string targetUserId = jReq.value("target_user_id", "");
        if (targetUserId.empty()) {
            targetUserId = sessOpt->user_id;
        }

        std::string accountType = jReq.value("account_type", "CHECKING");
        std::string customerPin = jReq.value("customer_pin", "");
        bool kycVerified = jReq.value("kyc_verified", false);
        int64_t initialBalanceVnd = jReq.value("initial_balance_vnd", 0LL);

        TransactionResult res = m_accountService.openNewAccount(sessOpt->user_id, targetUserId, accountType, customerPin, kycVerified, initialBalanceVnd);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// =========================================================================
// 4. API KHÓA / MỞ KHÓA TÀI KHOẢN KHẨN CẤP
// =========================================================================
QString AccountBridge::toggleAccountLock(const QString& jsonRequest) {
    try {
        json jReq = json::parse(jsonRequest.toStdString());
        std::string token = jReq.value("session_token", "");

        auto sessOpt = m_sessionManager.getSession(token);
        if (!sessOpt.has_value()) {
            json err = {{"success", false}, {"message", "Phiên đăng nhập đã hết hạn hoặc không hợp lệ!"}};
            return QString::fromStdString(err.dump());
        }

        std::string accountId = jReq.value("account_id", "");
        bool lockRequest = jReq.value("lock_request", true);
        std::string pin = jReq.value("pin", "");

        TransactionResult res = m_accountService.toggleAccountLock(sessOpt->user_id, accountId, lockRequest, pin);
        json jRes = res;
        return QString::fromStdString(jRes.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

} // namespace Banking
