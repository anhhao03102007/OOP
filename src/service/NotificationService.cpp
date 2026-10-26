#include "services/NotificationService.hpp"

namespace Banking {
NotificationsResult NotificationService::list(const std::string& sessionToken) {
    NotificationsResult result;
    const auto session = m_sessions.getSession(sessionToken);
    if (!session) { result.message = "Phiên đăng nhập đã hết hạn."; return result; }
    try {
        result.notifications = m_repository.listForUser(session->user_id);
        result.success = true;
        result.message = "Đã tải thông báo.";
    } catch (const std::exception& error) { result.message = error.what(); }
    return result;
}

UserOperationResult NotificationService::markRead(const MarkNotificationReadRequest& request) {
    UserOperationResult result;
    const auto session = m_sessions.getSession(request.session_token);
    if (!session) { result.message = "Phiên đăng nhập đã hết hạn."; return result; }
    if (request.notification_id.empty()) { result.message = "Thiếu mã thông báo."; return result; }
    if (!m_repository.markRead(session->user_id, request.notification_id)) {
        result.message = "Không cập nhật được trạng thái thông báo.";
        return result;
    }
    result.success = true;
    result.message = "Đã đánh dấu thông báo đã đọc.";
    return result;
}
}
