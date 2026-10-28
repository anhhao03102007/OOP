#ifndef TRANSACTION_BRIDGE_HPP
#define TRANSACTION_BRIDGE_HPP

#include <QObject>
#include <QString>

namespace Banking {

class TransactionService;

class TransactionBridge : public QObject {
    Q_OBJECT

private:
    TransactionService& m_service;

public:
    explicit TransactionBridge(TransactionService& service, QObject* parent = nullptr);

    // 1. API CHUYỂN TIỀN (NỘI BỘ & LIÊN NGÂN HÀNG)
    Q_INVOKABLE QString transfer(const QString& jsonRequest);
    Q_INVOKABLE QString requestOtp(const QString& jsonRequest);

    // 2. API RÚT TIỀN MẶT (ATM / TẠI QUẦY)
    Q_INVOKABLE QString withdraw(const QString& jsonRequest);

    // 3. API THANH TOÁN HÓA ĐƠN (ĐIỆN, NƯỚC, INTERNET...)
    Q_INVOKABLE QString payBill(const QString& jsonRequest);

    // 4. API NẠP TIỀN MẶT TẠI QUẦY (DÀNH CHO TELLER / ADMIN)
    Q_INVOKABLE QString depositCounter(const QString& jsonRequest);
    Q_INVOKABLE QString withdrawCounter(const QString& jsonRequest);
    Q_INVOKABLE QString topup(const QString& jsonRequest);
};

} // namespace Banking

#endif // TRANSACTION_BRIDGE_HPP
