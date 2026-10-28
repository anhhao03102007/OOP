#include <core/SessionManager.hpp>
#include <core/AuthSecurity.hpp>

namespace Banking {

SessionManager::SessionManager(std::chrono::seconds idleTimeout, std::chrono::seconds maxLifetime)
    : idle_(idleTimeout), maxLife_(maxLifetime) {}

std::string SessionManager::createSession(const std::string& userId, const std::string& fullName,
                                          const std::string& role, const std::string& primaryAccountId) {
    UserSessionDTO dto;
    dto.user_id = userId;
    dto.full_name = fullName;
    dto.role = role;
    dto.primary_account_id = primaryAccountId;
    dto.session_token = AuthSecurity::generateToken(32);
    const auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        it = (it->second.dto.user_id == userId) ? sessions_.erase(it) : std::next(it);
    }
    sessions_[dto.session_token] = Entry{dto, now, now};
    return dto.session_token;
}

std::optional<UserSessionDTO> SessionManager::getSession(const std::string& token) {
    if (token.empty()) return std::nullopt;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mu_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return std::nullopt;
    if (now - it->second.lastSeen > idle_ || now - it->second.created > maxLife_) {
        sessions_.erase(it);
        return std::nullopt;
    }
    it->second.lastSeen = now;
    return it->second.dto;
}

void SessionManager::revokeSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mu_);
    sessions_.erase(token);
}

size_t SessionManager::revokeAllSessionsForUser(const std::string& userId) {
    std::lock_guard<std::mutex> lock(mu_);
    size_t n = 0;
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.dto.user_id == userId) {
            it = sessions_.erase(it);
            ++n;
        } else {
            ++it;
        }
    }
    return n;
}

}  // namespace Banking