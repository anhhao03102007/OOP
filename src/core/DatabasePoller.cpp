#include "core/DatabasePoller.hpp"
#include <core/json.hpp>
#include <QDebug>

namespace Banking {

using json = nlohmann::json;

DatabasePoller::DatabasePoller(TursoDB& db, const QString& userId, const QString& accountId, QObject* parent)
    : QObject(parent), m_db(db), m_userId(userId), m_accountId(accountId) {}

DatabasePoller::~DatabasePoller() {
    stopPolling();
}

void DatabasePoller::startPolling(int intervalMs) {
    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &DatabasePoller::checkForUpdates);
    }
    m_timer->start(intervalMs);
    qDebug() << "[DatabasePoller] Đã bắt đầu Worker Thread quét CSDL mỗi" << intervalMs << "ms";
}

void DatabasePoller::stopPolling() {
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
        qDebug() << "[DatabasePoller] Đã dừng quét CSDL.";
    }
}

void DatabasePoller::checkForUpdates() {
    // ⚠️ Tác vụ này chạy hoàn toàn trên WORKER THREAD
    try {
        // Query 1: Lấy số dư và transaction_id mới nhất của tài khoản
        const std::string sql = "SELECT balance_vnd, last_tx_id FROM accounts WHERE account_id = ? LIMIT 1;";
        const json args = json::array({{{"type", "text"}, {"value", m_accountId.toStdString()}}});
        json response = m_db.execute(sql, args);

        // Giả định hàm parse dữ liệu từ TursoDB JSON response
        if (response.contains("results") && !response["results"].empty()) {
            auto rows = response["results"][0]["response"]["result"]["rows"];
            if (!rows.empty()) {
                int64_t currentBalance = rows[0][0]["value"].get<int64_t>();
                std::string currentTxId = rows[0][1]["value"].get<std::string>();

                // Khởi tạo lần đầu
                if (m_lastKnownBalance == -1) {
                    m_lastKnownBalance = currentBalance;
                    m_lastTransactionId = currentTxId;
                    return;
                }

                // Kiểm tra xem có sự thay đổi số dư hoặc giao dịch mới từ bên ngoài không
                if (currentBalance != m_lastKnownBalance || currentTxId != m_lastTransactionId) {
                    m_lastKnownBalance = currentBalance;
                    m_lastTransactionId = currentTxId;

                    // Đóng gói thông tin biến động số dư
                    json txInfo = {
                        {"account_id", m_accountId.toStdString()},
                        {"new_balance", currentBalance},
                        {"tx_id", currentTxId},
                        {"timestamp", "Mới xong"}
                    };

                    qDebug() << "[DatabasePoller] Phát hiện biến động từ CSDL Cloud!";
                    
                    // Phát Signal báo về Main Thread / AccountBridge
                    emit balanceUpdated(currentBalance, QString::fromStdString(txInfo.dump()));
                }
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "[DatabasePoller] Lỗi truy vấn CSDL:" << e.what();
    }
}

} // namespace Banking
