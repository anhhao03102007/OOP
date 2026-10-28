#ifndef NOTIFICATION_SERVICE_HPP
#define NOTIFICATION_SERVICE_HPP

#include "core/SessionManager.hpp"
#include "dtos/NotificationDTOs.hpp"
#include "dtos/UserDTOs.hpp"
#include "repository/NotificationRepository.hpp"

namespace Banking {
class NotificationService {
    NotificationRepository& m_repository;
    SessionManager& m_sessions;
public:
    NotificationService(NotificationRepository& repository, SessionManager& sessions)
        : m_repository(repository), m_sessions(sessions) {}
    NotificationsResult list(const std::string& sessionToken);
    UserOperationResult markRead(const MarkNotificationReadRequest& request);
};
}

#endif
