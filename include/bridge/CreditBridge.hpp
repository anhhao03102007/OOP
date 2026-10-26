#ifndef CREDIT_BRIDGE_HPP
#define CREDIT_BRIDGE_HPP
#include <QObject>
#include <QString>
namespace Banking {
class CreditService;
class CreditBridge : public QObject {
    Q_OBJECT
    CreditService& service;
public:
    explicit CreditBridge(CreditService& value, QObject* parent=nullptr);
    Q_INVOKABLE QString getMyAccount(const QString& request);
    Q_INVOKABLE QString getApplications(const QString& request);
    Q_INVOKABLE QString apply(const QString& request);
    Q_INVOKABLE QString submitDocument(const QString& request);
    Q_INVOKABLE QString reviewDocument(const QString& request);
    Q_INVOKABLE QString decide(const QString& request);
    Q_INVOKABLE QString spend(const QString& request);
    Q_INVOKABLE QString repay(const QString& request);
};
}
#endif
