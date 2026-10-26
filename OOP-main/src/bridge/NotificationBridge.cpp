#include "bridge/NotificationBridge.hpp"
#include "core/json.hpp"
#include "dtos/NotificationDTOs.hpp"
#include "services/NotificationService.hpp"

namespace Banking {
using json = nlohmann::json;
NotificationBridge::NotificationBridge(NotificationService& service, QObject* parent)
    : QObject(parent), m_service(service) {}

QString NotificationBridge::getNotifications(const QString& request) {
    try {
        const json input = json::parse(request.toStdString());
        return QString::fromStdString(json(m_service.list(input.value("session_token", ""))).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu thông báo không hợp lệ."}}).dump());
    }
}

QString NotificationBridge::markRead(const QString& request) {
    try {
        const auto input = json::parse(request.toStdString()).get<MarkNotificationReadRequest>();
        return QString::fromStdString(json(m_service.markRead(input)).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu cập nhật thông báo không hợp lệ."}}).dump());
    }
}
}
