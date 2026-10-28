#ifndef NOTIFICATION_DTOS_HPP
#define NOTIFICATION_DTOS_HPP

#include <string>
#include <vector>
#include <core/json.hpp>

namespace Banking {
using json = nlohmann::json;

struct NotificationDTO {
    std::string notification_id;
    std::string title;
    std::string body;
    std::string notification_type;
    std::string reference_id;
    bool unread = true;
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(NotificationDTO, notification_id, title, body,
                                   notification_type, reference_id, unread, created_at)
};

struct NotificationsResult {
    bool success = false;
    std::string message;
    std::vector<NotificationDTO> notifications;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(NotificationsResult, success, message, notifications)
};

struct MarkNotificationReadRequest {
    std::string session_token;
    std::string notification_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MarkNotificationReadRequest, session_token, notification_id)
};
}

#endif
