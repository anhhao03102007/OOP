#ifndef NOTIFICATION_REPOSITORY_HPP
#define NOTIFICATION_REPOSITORY_HPP

#include <string>
#include <vector>
#include "core/TursoDB.hpp"
#include "dtos/NotificationDTOs.hpp"

namespace Banking {
class NotificationRepository {
    TursoDB& m_database;
public:
    explicit NotificationRepository(TursoDB& database) : m_database(database) {}
    std::vector<NotificationDTO> listForUser(const std::string& userId, int limit = 50);
    bool prepareTransactionNotifications();
    bool markRead(const std::string& userId, const std::string& notificationId);
    bool create(const std::string& userId, const std::string& title, const std::string& body,
                const std::string& type, const std::string& referenceId);
};
}

#endif
