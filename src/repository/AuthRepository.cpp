#include "repository/AuthRepository.hpp"
#include <ctime>

namespace Banking {

AuthRepository::AuthRepository(TursoDB& tursoDb) : m_tursoDb(tursoDb) {}

std::string AuthRepository::extractCell(const json& cell) {
    if (cell.is_object() && cell.contains("value") && !cell["value"].is_null()) {
        const json& value = cell["value"];
        return value.is_string() ? value.get<std::string>() : value.dump();
    }
    return "";
}

std::shared_ptr<User> AuthRepository::mapRowToUser(const json& r) {
    std::string userId       = extractCell(r[0]);
    std::string username     = extractCell(r[1]);
    std::string passwordHash = extractCell(r[2]);
    std::string salt         = extractCell(r[3]);
    std::string fullName     = extractCell(r[4]);
    std::string email        = extractCell(r[5]);
    std::string phone        = extractCell(r[6]);
    std::string cccd         = extractCell(r[7]);
    std::string role         = extractCell(r[8]);
    std::string status       = extractCell(r[9]);

    std::shared_ptr<User> user;
    if (role == "TELLER") {
        user = std::make_shared<TellerUser>(userId, username, passwordHash, salt, fullName, email, phone, cccd, status);
    } else if (role == "ADMIN") {
        user = std::make_shared<AdminUser>(userId, username, passwordHash, salt, fullName, email, phone, cccd, status);
    } else {
        user = std::make_shared<CustomerUser>(userId, username, passwordHash, salt, fullName, email, phone, cccd, status);
    }

    if (r.size() > 10) {
        user->setPinHash(extractCell(r[10]));
    }
    return user;
}

// 1. Tra cứu người dùng qua Username / Email / Phone / CCCD
std::shared_ptr<User> AuthRepository::findByIdentifier(const std::string& identifier) {
    std::string sql = 
        "SELECT user_id, username, password_hash, salt, full_name, email, phone, cccd, role, status, pin_hash "
        "FROM users WHERE username = ? OR email = ? OR phone = ? OR cccd = ? LIMIT 1;";

    json args = json::array({
        {{"type", "text"}, {"value", identifier}},
        {{"type", "text"}, {"value", identifier}},
        {{"type", "text"}, {"value", identifier}},
        {{"type", "text"}, {"value", identifier}}
    });

    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                return mapRowToUser(rows[0]);
            }
        }
    } catch (...) {}
    return nullptr;
}

// 2. Kiểm tra tồn tại trùng lặp thông tin khi đăng ký
bool AuthRepository::existsByIdentifier(const std::string& val) {
    std::string sql = "SELECT COUNT(*) FROM users WHERE username = ? OR email = ? OR phone = ? OR cccd = ? LIMIT 1;";
    json args = json::array({
        {{"type", "text"}, {"value", val}},
        {{"type", "text"}, {"value", val}},
        {{"type", "text"}, {"value", val}},
        {{"type", "text"}, {"value", val}}
    });

    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                return std::stoi(extractCell(rows[0][0])) > 0;
            }
        }
    } catch (...) {}
    return false;
}

// 3. Lấy số tài khoản thanh toán mặc định của User
std::string AuthRepository::getPrimaryAccountId(const std::string& userId) {
    std::string sql = "SELECT account_id FROM accounts WHERE user_id = ? AND account_type = 'CHECKING' ORDER BY created_at ASC, account_id ASC LIMIT 1;";
    json args = json::array({{{"type", "text"}, {"value", userId}}});
    json res = m_tursoDb.execute(sql, args);

    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                return extractCell(rows[0][0]);
            }
        }
    } catch (...) {}
    return "";
}

// 4. Tạo người dùng khách hàng mới
bool AuthRepository::createCustomer(const std::string& userId, const std::string& username, const std::string& passHash,
                                    const std::string& salt, const std::string& fullname, const std::string& email,
                                    const std::string& cccd, const std::string& phone) {
    std::string sql = 
        "INSERT INTO users (user_id, username, password_hash, salt, full_name, email, cccd, phone, role, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'CUSTOMER', 'ACTIVE');";

    json args = json::array({
        {{"type", "text"}, {"value", userId}},
        {{"type", "text"}, {"value", username}},
        {{"type", "text"}, {"value", passHash}},
        {{"type", "text"}, {"value", salt}},
        {{"type", "text"}, {"value", fullname}},
        {{"type", "text"}, {"value", email}},
        {{"type", "text"}, {"value", cccd}},
        {{"type", "text"}, {"value", phone}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

bool AuthRepository::createCustomerWithCheckingAccount(const std::string& userId, const std::string& username,
                                                       const std::string& passHash, const std::string& salt,
                                                       const std::string& fullname, const std::string& email,
                                                       const std::string& cccd, const std::string& phone,
                                                       const std::string& accountId) {
    const std::string userSql =
        "INSERT INTO users (user_id, username, password_hash, salt, full_name, email, cccd, phone, role, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'CUSTOMER', 'ACTIVE');";
    const json userArgs = json::array({
        {{"type", "text"}, {"value", userId}},
        {{"type", "text"}, {"value", username}},
        {{"type", "text"}, {"value", passHash}},
        {{"type", "text"}, {"value", salt}},
        {{"type", "text"}, {"value", fullname}},
        {{"type", "text"}, {"value", email}},
        {{"type", "text"}, {"value", cccd}},
        {{"type", "text"}, {"value", phone}}
    });

    const std::string accountSql =
        "INSERT INTO accounts (account_id, user_id, balance_vnd, account_type, status, created_at) "
        "VALUES (?, ?, 0, 'CHECKING', 'ACTIVE', CURRENT_TIMESTAMP);";
    const json accountArgs = json::array({
        {{"type", "text"}, {"value", accountId}},
        {{"type", "text"}, {"value", userId}}
    });

    const json res = m_tursoDb.executeBatch({{userSql, userArgs}, {accountSql, accountArgs}});
    if (!res.contains("results") || !res["results"].is_array() || res["results"].size() < 2) {
        return false;
    }

    for (const auto& result : res["results"]) {
        if (!result.is_object() || result.value("type", "") != "ok") {
            return false;
        }
    }
    return true;
}

// 5. Cập nhật mật khẩu băm & Salt mới (Dùng cho Quên mật khẩu)
bool AuthRepository::updatePassword(const std::string& userId, const std::string& newPassHash, const std::string& newSalt) {
    std::string sql = "UPDATE users SET password_hash = ?, salt = ?, failed_attempts = 0, status = 'ACTIVE' WHERE user_id = ?;";
    json args = json::array({
        {{"type", "text"}, {"value", newPassHash}},
        {{"type", "text"}, {"value", newSalt}},
        {{"type", "text"}, {"value", userId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

// 6. Quản lý đếm đăng nhập sai & Khóa tài khoản
void AuthRepository::updateFailedAttempts(const std::string& userId, int count) {
    std::string sql = "UPDATE users SET failed_attempts = ? WHERE user_id = ?;";
    json args = json::array({
        {{"type", "numeric"}, {"value", count}},
        {{"type", "text"},    {"value", userId}}
    });
    m_tursoDb.execute(sql, args);
}

void AuthRepository::lockUserTemporarily(const std::string& userId, int minutes) {
    std::time_t unlockTime = std::time(nullptr) + (minutes * 60);
    std::string sql = "UPDATE users SET status = 'BLOCKED', locked_until = ? WHERE user_id = ?;";
    json args = json::array({
        {{"type", "numeric"}, {"value", unlockTime}},
        {{"type", "text"},    {"value", userId}}
    });
    m_tursoDb.execute(sql, args);
}

bool AuthRepository::releaseExpiredTemporaryLock(const std::string& userId) {
    const auto now = static_cast<int64_t>(std::time(nullptr));
    const std::string sql = "UPDATE users SET status = 'ACTIVE', failed_attempts = 0, locked_until = 0 WHERE user_id = ? AND status = 'BLOCKED' AND locked_until > 0 AND locked_until <= ?;";
    const json args = json::array({{{"type", "text"}, {"value", userId}}, {{"type", "numeric"}, {"value", now}}});
    const json response = m_tursoDb.execute(sql, args);
    return response.contains("results") && !response["results"].empty() && response["results"][0].value("type", "") == "ok";
}

void AuthRepository::resetFailedAttempts(const std::string& userId) {
    std::string sql = "UPDATE users SET failed_attempts = 0, locked_until = 0 WHERE user_id = ?;";
    json args = json::array({{{"type", "text"}, {"value", userId}}});
    m_tursoDb.execute(sql, args);
}

int64_t AuthRepository::getBalance(const std::string& accountId) {
    if (accountId.empty()) {
        return 0;
    }

    std::string sql = "SELECT balance_vnd FROM accounts WHERE account_id = ? LIMIT 1;";
    json args = json::array({{{"type", "text"}, {"value", accountId}}});
    json res = m_tursoDb.execute(sql, args);

    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                std::string balanceStr = extractCell(rows[0][0]);
                if (!balanceStr.empty()) {
                    return std::stoll(balanceStr);
                }
            }
        }
    } catch (...) {}

    return 0;
}

} // namespace Banking
