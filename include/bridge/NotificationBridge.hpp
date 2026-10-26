#ifndef NOTIFICATION_BRIDGE_HPP
#define NOTIFICATION_BRIDGE_HPP

#include <QObject>
#include <QString>

namespace Banking {
class NotificationService;
class NotificationBridge : public QObject {
    Q_OBJECT
    NotificationService& m_service;
public:
    explicit NotificationBridge(NotificationService& service, QObject* parent = nullptr);
    Q_INVOKABLE QString getNotifications(const QString& jsonRequest);
    Q_INVOKABLE QString markRead(const QString& jsonRequest);
};
}

#endif
