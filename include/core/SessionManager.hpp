#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <dtos/AuthDTOs.hpp>

namespace Banking {

// Phiên đăng nhập trên RAM. Mỗi người dùng chỉ có một phiên; phiên hết hạn khi nhàn rỗi hoặc quá tuổi thọ tối đa.
class SessionManager {
public:
    explicit SessionManager(std::chrono::seconds idleTimeout = std::chrono::minutes(15),
                            std::chrono::seconds maxLifetime = std::chrono::hours(8));

    // Tạo phiên mới (thu hồi phiên cũ của cùng người dùng), trả về token ngẫu nhiên 64 ký tự hex.
    std::string createSession(const std::string& userId, const std::string& fullName, const std::string& role,
                              const std::string& primaryAccountId);

    // Trả phiên nếu còn hiệu lực (và gia hạn thời gian nhàn rỗi).
    std::optional<UserSessionDTO> getSession(const std::string& token);

    void revokeSession(const std::string& token);
    size_t revokeAllSessionsForUser(const std::string& userId);  // dùng khi khóa tài khoản

private:
    struct Entry {
        UserSessionDTO dto;
        std::chrono::steady_clock::time_point created;
        std::chrono::steady_clock::time_point lastSeen;
    };
    std::chrono::seconds idle_;
    std::chrono::seconds maxLife_;
    std::mutex mu_;
    std::unordered_map<std::string, Entry> sessions_;
};

}  // namespace Banking

#endif // SESSION_MANAGER_HPP