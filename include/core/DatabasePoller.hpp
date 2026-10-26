#ifndef DATABASE_POLLER_HPP
#define DATABASE_POLLER_HPP

#include <QObject>
#include <QTimer>
#include <QString>
#include <cstdint>
#include "core/TursoDB.hpp"

namespace Banking {

class DatabasePoller : public QObject {
    Q_OBJECT

private:
    TursoDB& m_db;
    QString m_userId;
    QString m_accountId;
    int64_t m_lastKnownBalance = -1;
    std::string m_lastTransactionId = "";
    QTimer* m_timer = nullptr;

public:
    explicit DatabasePoller(TursoDB& db, const QString& userId, const QString& accountId, QObject* parent = nullptr);
    ~DatabasePoller();

public slots:
    // Slot khởi tạo Timer khi Thread bắt đầu
    void startPolling(int intervalMs = 3000); // Mặc định 3 giây
    void stopPolling();

private slots:
    // Hàm thực thi truy vấn CSDL định kỳ
    void checkForUpdates();

signals:
    // Signal bắn ra khi phát hiện số dư hoặc giao dịch mới
    void balanceUpdated(int64_t newBalanceVnd, QString transactionJson);
    void errorOccurred(QString errorMessage);
};

} // namespace Banking

#endif // DATABASE_POLLER_HPP