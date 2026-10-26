#ifndef ACCOUNT_BRIDGE_HPP
#define ACCOUNT_BRIDGE_HPP

#include <QObject>
#include <QString>
#include "services/AccountService.hpp"
#include "core/SessionManager.hpp"

namespace Banking {

class AccountBridge : public QObject {
    Q_OBJECT

private:
    AccountService& m_accountService;
    SessionManager& m_sessionManager;

public:
    explicit AccountBridge(AccountService& accountService, 
                           SessionManager& sessionManager, 
                           QObject* parent = nullptr);

    // 1. API LẤY DANH SÁCH TÀI KHOẢN (TRUY VẤN SỐ DƯ)
    Q_INVOKABLE QString getAccounts(const QString& jsonRequest);

    // 2. API TRUY VẤN LỊCH SỬ GIAO DỊCH (SAO KÊ TÀI KHOẢN)
    Q_INVOKABLE QString getTransactionHistory(const QString& jsonRequest);

    // 3. API MỞ TÀI KHOẢN MỚI (CHECKING / SAVINGS / CREDIT / LOAN)
    Q_INVOKABLE QString openNewAccount(const QString& jsonRequest);

    // 4. API KHÓA / MỞ KHÓA TÀI KHOẢN KHẨN CẤP
    Q_INVOKABLE QString toggleAccountLock(const QString& jsonRequest);
};

} // namespace Banking

#endif // ACCOUNT_BRIDGE_HPP