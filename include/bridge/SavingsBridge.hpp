#ifndef SAVINGS_BRIDGE_HPP
#define SAVINGS_BRIDGE_HPP

#include <QObject>
#include <QString>

namespace Banking {

class SavingsService;

class SavingsBridge : public QObject {
    Q_OBJECT

private:
    SavingsService& m_savingsService;

public:
    explicit SavingsBridge(SavingsService& savingsService, QObject* parent = nullptr);

    // 1. API Mở sổ tiết kiệm
    Q_INVOKABLE QString openSavings(const QString& jsonRequest);

    // 2. API Tất toán sổ tiết kiệm
    Q_INVOKABLE QString closeSavings(const QString& jsonRequest);
};

} // namespace Banking

#endif // SAVINGS_BRIDGE_HPP
