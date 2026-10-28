#include "repository/NotificationRepository.hpp"
#include "core/AuthSecurity.hpp"

#include <algorithm>
#include <stdexcept>

namespace Banking {
namespace {
std::string cellText(const json& cell) {
    if (!cell.is_object() || !cell.contains("value") || cell["value"].is_null()) return {};
    const auto& value = cell["value"];
    return value.is_string() ? value.get<std::string>() : value.dump();
}
bool queryOk(const json& response) {
    return response.contains("results") && response["results"].is_array() && !response["results"].empty() &&
           response["results"][0].is_object() && response["results"][0].value("type", "") == "ok";
}
}

std::vector<NotificationDTO> NotificationRepository::listForUser(const std::string& userId, int limit) {
    const json synced = m_database.execute(
        "WITH user_tx AS ("
        "SELECT t.* FROM accounts a JOIN transactions t ON t.sender_account_id = a.account_id "
        "WHERE a.user_id = ? AND t.status = 'SUCCESS' "
        "UNION "
        "SELECT t.* FROM accounts a JOIN transactions t ON t.receiver_account_id = a.account_id "
        "WHERE a.user_id = ? AND t.status = 'SUCCESS') "
        "INSERT OR IGNORE INTO notifications (notification_id, user_id, title, body, notification_type, reference_id, created_at) "
        "SELECT 'NTF_TX_' || t.transaction_id || '_' || ?, ?, "
        "CASE "
        "WHEN t.type = 'TRANSFER' AND EXISTS (SELECT 1 FROM accounts r WHERE r.account_id = t.receiver_account_id AND r.user_id = ?) THEN 'Bạn vừa nhận tiền' "
        "WHEN t.type = 'TRANSFER' THEN 'Chuyển tiền thành công' "
        "WHEN t.type = 'LOAN_DISBURSEMENT' THEN 'Khoản vay đã được giải ngân' "
        "WHEN t.type = 'LOAN_REPAYMENT' THEN 'Đã thanh toán khoản vay' "
        "WHEN t.type = 'CREDIT_SPEND' THEN 'Giao dịch tín dụng' "
        "WHEN t.type = 'CREDIT_REPAYMENT' THEN 'Đã thanh toán dư nợ tín dụng' "
        "WHEN t.type = 'CREDIT_INTEREST' THEN 'Biến động dư nợ tín dụng' "
        "WHEN t.type IN ('WITHDRAW','CASH_WITHDRAW') THEN 'Rút tiền thành công' "
        "WHEN t.type IN ('DEPOSIT','CASH_DEPOSIT','DEPOSIT_COUNTER') THEN 'Tiền đã vào tài khoản' "
        "WHEN t.type = 'BILL_PAYMENT' THEN 'Thanh toán hóa đơn thành công' "
        "WHEN t.type = 'MOBILE_TOPUP' THEN 'Nạp tiền điện thoại thành công' "
        "WHEN t.type = 'SAVINGS_OPEN' THEN 'Gửi tiết kiệm thành công' "
        "WHEN t.type = 'SAVINGS_CLOSE' THEN 'Tất toán tiết kiệm thành công' "
        "ELSE 'Giao dịch thành công' END, "
        "COALESCE(NULLIF(t.description, ''), t.type) || ' · Số tiền: ' || "
        "CAST(t.amount_vnd AS TEXT) || ' VND · Mã giao dịch: ' || t.transaction_id || '.' "
        "|| CASE WHEN t.fee_vnd > 0 THEN ' Phí: ' || CAST(t.fee_vnd AS TEXT) || ' VND.' ELSE '' END, "
        "'TRANSACTION', t.transaction_id, t.created_at "
        "FROM user_tx t "
        "WHERE NOT EXISTS (SELECT 1 FROM notifications n WHERE n.user_id = ? AND n.reference_id = t.transaction_id) "
        "ORDER BY t.created_at, t.transaction_id;",
        json::array({{{"type", "text"}, {"value", userId}}, {{"type", "text"}, {"value", userId}},
                     {{"type", "text"}, {"value", userId}}, {{"type", "text"}, {"value", userId}},
                     {{"type", "text"}, {"value", userId}}, {{"type", "text"}, {"value", userId}}}));
    if (!queryOk(synced)) throw std::runtime_error("Không thể đồng bộ thông báo từ lịch sử giao dịch.");

    const json response = m_database.execute(
        "SELECT notification_id, title, body, notification_type, reference_id, read_at, created_at "
        "FROM notifications WHERE user_id = ? ORDER BY created_at DESC LIMIT ?;",
        json::array({{{"type", "text"}, {"value", userId}}, {{"type", "numeric"}, {"value", limit}}}));
    if (!queryOk(response)) throw std::runtime_error("Không tải được thông báo từ database.");
    const auto rows = response["results"][0]["response"]["result"]["rows"];
    if (!rows.is_array()) throw std::runtime_error("Dữ liệu thông báo không hợp lệ.");
    std::vector<NotificationDTO> items;
    for (const auto& row : rows) {
        if (!row.is_array() || row.size() < 7) throw std::runtime_error("Bản ghi thông báo không đầy đủ.");
        items.push_back({cellText(row[0]), cellText(row[1]), cellText(row[2]), cellText(row[3]),
                         cellText(row[4]), cellText(row[5]).empty(), cellText(row[6])});
    }
    return items;
}

bool NotificationRepository::prepareTransactionNotifications() {
    const json response = m_database.executeBatch({
        {"DROP TRIGGER IF EXISTS trg_accounts_notify_balance_change;", json::array()},
        {"CREATE INDEX IF NOT EXISTS idx_notifications_user_reference ON notifications(user_id, reference_id);", json::array()}
    });
    if (!response.contains("results") || !response["results"].is_array() || response["results"].empty()) return false;
    return std::all_of(response["results"].begin(), response["results"].end(), [](const json& item) {
        return item.is_object() && item.value("type", "") == "ok";
    });
}

bool NotificationRepository::markRead(const std::string& userId, const std::string& notificationId) {
    const json response = m_database.execute(
        "UPDATE notifications SET read_at = CURRENT_TIMESTAMP WHERE user_id = ? AND notification_id = ? AND read_at IS NULL;",
        json::array({{{"type", "text"}, {"value", userId}}, {{"type", "text"}, {"value", notificationId}}}));
    return queryOk(response);
}

bool NotificationRepository::create(const std::string& userId, const std::string& title, const std::string& body,
                                    const std::string& type, const std::string& referenceId) {
    const std::string id = "NTF_" + AuthSecurity::generateNumericCode(24);
    const json response = m_database.execute(
        "INSERT INTO notifications (notification_id, user_id, title, body, notification_type, reference_id) VALUES (?, ?, ?, ?, ?, ?);",
        json::array({{{"type", "text"}, {"value", id}}, {{"type", "text"}, {"value", userId}},
                     {{"type", "text"}, {"value", title}}, {{"type", "text"}, {"value", body}},
                     {{"type", "text"}, {"value", type}}, {{"type", "text"}, {"value", referenceId}}}));
    return queryOk(response);
}
}
