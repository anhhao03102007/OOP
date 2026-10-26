#include "repository/UserRepository.hpp"
#include <stdexcept>

namespace Banking {

UserRepository::UserRepository(TursoDB& tursoDb) : m_tursoDb(tursoDb) {}

std::string UserRepository::extractCell(const json& cell) {
    if (cell.is_object() && cell.contains("value") && !cell["value"].is_null()) {
        const auto& value = cell["value"];
        return value.is_string() ? value.get<std::string>() : value.dump();
    }
    return "";
}

std::shared_ptr<User> UserRepository::mapRowToUser(const json& r) {
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

// 1. Tìm người dùng theo user_id
std::shared_ptr<User> UserRepository::findById(const std::string& userId) {
    std::string sql = 
        "SELECT user_id, username, password_hash, salt, full_name, email, phone, cccd, role, status, pin_hash "
        "FROM users WHERE user_id = ? LIMIT 1;";

    json args = json::array({{{"type", "text"}, {"value", userId}}});
    json res = m_tursoDb.execute(sql, args);

    if (!res.contains("results") || !res["results"].is_array() || res["results"].empty() ||
        !res["results"][0].is_object() || res["results"][0].value("type", "") != "ok") {
        throw std::runtime_error("Không thể tải người dùng từ database.");
    }
    const auto& rows = res["results"][0]["response"]["result"]["rows"];
    if (!rows.is_array()) throw std::runtime_error("Database trả về dữ liệu người dùng không hợp lệ.");
    if (rows.empty()) return nullptr;
    if (!rows[0].is_array() || rows[0].size() < 11) {
        throw std::runtime_error("Bản ghi người dùng không đầy đủ.");
    }
    return mapRowToUser(rows[0]);
}

// 2. Tìm người dùng theo Mã tài khoản thanh toán (account_id)
std::shared_ptr<User> UserRepository::findByAccountId(const std::string& accountId) {
    std::string sql = 
        "SELECT u.user_id, u.username, u.password_hash, u.salt, u.full_name, u.email, u.phone, u.cccd, u.role, u.status, u.pin_hash "
        "FROM users u INNER JOIN accounts a ON u.user_id = a.user_id WHERE a.account_id = ? LIMIT 1;";

    json args = json::array({{{"type", "text"}, {"value", accountId}}});
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

// 3. Tra cứu Khách hàng (Dành cho Teller search theo Tên, CCCD, SĐT, Email)
std::vector<std::shared_ptr<User>> UserRepository::searchCustomers(const std::string& query) {
    std::vector<std::shared_ptr<User>> results;
    std::string sql = 
        "SELECT user_id, username, password_hash, salt, full_name, email, phone, cccd, role, status, pin_hash "
        "FROM users WHERE role = 'CUSTOMER' AND "
        "(full_name LIKE ? OR cccd LIKE ? OR phone LIKE ? OR email LIKE ?) LIMIT 20;";

    std::string pattern = "%" + query + "%";
    json args = json::array({
        {{"type", "text"}, {"value", pattern}},
        {{"type", "text"}, {"value", pattern}},
        {{"type", "text"}, {"value", pattern}},
        {{"type", "text"}, {"value", pattern}}
    });

    json res = m_tursoDb.execute(sql, args);
    if (!res.contains("results") || !res["results"].is_array() || res["results"].empty() ||
        !res["results"][0].is_object() || res["results"][0].value("type", "") != "ok") {
        throw std::runtime_error("Không thể tra cứu khách hàng từ database.");
    }
    const auto& rows = res["results"][0]["response"]["result"]["rows"];
    if (!rows.is_array()) throw std::runtime_error("Database trả về danh sách khách hàng không hợp lệ.");
    for (const auto& row : rows) {
        if (!row.is_array() || row.size() < 11) {
            throw std::runtime_error("Bản ghi khách hàng không đầy đủ.");
        }
        results.push_back(mapRowToUser(row));
    }
    return results;
}

std::vector<std::shared_ptr<User>> UserRepository::findStaffMembers() {
    std::vector<std::shared_ptr<User>> staff;
    const std::string sql =
        "SELECT user_id, username, password_hash, salt, full_name, email, phone, cccd, role, status, pin_hash "
        "FROM users WHERE role IN ('TELLER', 'ADMIN') ORDER BY role, full_name;";
    const json response = m_tursoDb.execute(sql);
    if (!response.contains("results") || !response["results"].is_array() || response["results"].empty() ||
        !response["results"][0].is_object() || response["results"][0].value("type", "") != "ok") {
        throw std::runtime_error("Không thể tải danh sách nhân viên từ database.");
    }
    const auto& rows = response["results"][0]["response"]["result"]["rows"];
    if (!rows.is_array()) throw std::runtime_error("Database trả về danh sách nhân viên không hợp lệ.");
    for (const auto& row : rows) {
        if (!row.is_array() || row.size() < 11) {
            throw std::runtime_error("Bản ghi nhân viên không đầy đủ.");
        }
        staff.push_back(mapRowToUser(row));
    }
    return staff;
}

bool UserRepository::createStaff(const std::string& userId, const std::string& username,
                                 const std::string& passwordHash, const std::string& salt,
                                 const std::string& fullname, const std::string& email,
                                 const std::string& phone, const std::string& cccd,
                                 const std::string& role) {
    const std::string sql =
        "INSERT INTO users (user_id, username, password_hash, salt, full_name, email, phone, cccd, role, status, pin_hash, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'ACTIVE', '', CURRENT_TIMESTAMP);";
    const json args = json::array({
        {{"type", "text"}, {"value", userId}},
        {{"type", "text"}, {"value", username}},
        {{"type", "text"}, {"value", passwordHash}},
        {{"type", "text"}, {"value", salt}},
        {{"type", "text"}, {"value", fullname}},
        {{"type", "text"}, {"value", email}},
        {{"type", "text"}, {"value", phone}},
        {{"type", "text"}, {"value", cccd}},
        {{"type", "text"}, {"value", role}}
    });
    const json response = m_tursoDb.execute(sql, args);
    return response.contains("results") && !response["results"].empty() &&
           response["results"][0].value("type", "") == "ok";
}

// 4. Cập nhật thông tin cá nhân
bool UserRepository::updateProfile(const std::string& userId, const std::string& fullname, const std::string& email, const std::string& phone) {
    std::string sql = "UPDATE users SET full_name = ?, email = ?, phone = ? WHERE user_id = ?;";
    json args = json::array({
        {{"type", "text"}, {"value", fullname}},
        {{"type", "text"}, {"value", email}},
        {{"type", "text"}, {"value", phone}},
        {{"type", "text"}, {"value", userId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

// 5. Cập nhật mật khẩu băm mới
bool UserRepository::updatePassword(const std::string& userId, const std::string& newPasswordHash, const std::string& newSalt) {
    std::string sql = "UPDATE users SET password_hash = ?, salt = ? WHERE user_id = ?;";
    json args = json::array({
        {{"type", "text"}, {"value", newPasswordHash}},
        {{"type", "text"}, {"value", newSalt}},
        {{"type", "text"}, {"value", userId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

// 6. Cập nhật mã PIN băm giao dịch (6 số)
bool UserRepository::updatePin(const std::string& userId, const std::string& pinHash) {
    std::string sql = "UPDATE users SET pin_hash = ? WHERE user_id = ?;";
    json args = json::array({
        {{"type", "text"}, {"value", pinHash}},
        {{"type", "text"}, {"value", userId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

// 7. Khóa/Mở khóa trạng thái tài khoản người dùng
bool UserRepository::updateStatus(const std::string& userId, const std::string& newStatus) {
    std::string sql = "UPDATE users SET status = ? WHERE user_id = ?;";
    json args = json::array({
        {{"type", "text"}, {"value", newStatus}},
        {{"type", "text"}, {"value", userId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

// 8. Lấy mã PIN hash hiện tại để xác thực
std::string UserRepository::getPinHash(const std::string& userId) {
    std::string sql = "SELECT pin_hash FROM users WHERE user_id = ? LIMIT 1;";
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

} // namespace Banking
