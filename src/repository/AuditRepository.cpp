#include "repository/AuditRepository.hpp"
#include <ctime>
#include <cstdlib>
#include <stdexcept>
#include <core/AuthSecurity.hpp>

namespace Banking {

AuditRepository::AuditRepository(TursoDB& tursoDb) : m_tursoDb(tursoDb) {}

std::string AuditRepository::extractCell(const nlohmann::json& cell) {
    if (cell.is_object() && cell.contains("value") && !cell["value"].is_null()) {
        const auto& value = cell["value"];
        return value.is_string() ? value.get<std::string>() : value.dump();
    }
    return "";
}

std::string AuditRepository::getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    return std::string(buf);
}

// 1. Ghi nhận vết hành động (Append-Only Transaction)
bool AuditRepository::logAction(const std::string& operatorId, const std::string& action,
                                const std::string& targetId, const std::string& details) {
    std::string logId = "LOG_" + AuthSecurity::generateNumericCode(20);
    return logActionWithId(logId, operatorId, action, targetId, details);
}

bool AuditRepository::logActionWithId(const std::string& logId, const std::string& operatorId,
                                      const std::string& action, const std::string& targetId,
                                      const std::string& details) {
    const std::string nowStr = getCurrentTimestamp();

    const std::string sql =
        "INSERT OR IGNORE INTO audit_logs (log_id, operator_id, action, target_id, details, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    nlohmann::json args = nlohmann::json::array({
        {{"type", "text"}, {"value", logId}},
        {{"type", "text"}, {"value", operatorId}},
        {{"type", "text"}, {"value", action}},
        {{"type", "text"}, {"value", targetId}},
        {{"type", "text"}, {"value", details}},
        {{"type", "text"}, {"value", nowStr}}
    });

    const nlohmann::json res = m_tursoDb.execute(sql, args);
    if (!res.contains("results") || res["results"].empty() ||
        !res["results"][0].is_object() || res["results"][0].value("type", "") != "ok") {
        return false;
    }

    // INSERT OR IGNORE also makes bridge-level error handling safe if the service
    // already wrote the execution record before an exception was returned.
    const nlohmann::json check = m_tursoDb.execute(
        "SELECT 1 FROM audit_logs WHERE log_id = ? LIMIT 1;",
        nlohmann::json::array({{{"type", "text"}, {"value", logId}}}));
    try {
        const auto& rows = check["results"][0]["response"]["result"]["rows"];
        return rows.is_array() && !rows.empty();
    } catch (...) {
        return false;
    }
}

std::optional<AuditLogItemDTO> AuditRepository::getLogById(const std::string& logId) {
    const nlohmann::json response = m_tursoDb.execute(
        "SELECT log_id, operator_id, action, target_id, details, created_at "
        "FROM audit_logs WHERE log_id = ? LIMIT 1;",
        nlohmann::json::array({{{"type", "text"}, {"value", logId}}}));
    try {
        const auto& rows = response["results"][0]["response"]["result"]["rows"];
        if (!rows.is_array() || rows.empty() || !rows[0].is_array() || rows[0].size() < 6) {
            return std::nullopt;
        }
        const auto& row = rows[0];
        return AuditLogItemDTO{extractCell(row[0]), extractCell(row[1]), extractCell(row[2]),
                               extractCell(row[3]), extractCell(row[4]), extractCell(row[5])};
    } catch (...) {
        return std::nullopt;
    }
}

// 2. Tra cứu danh sách Audit Log có Phân Trang dành cho Admin (LIMIT / OFFSET)
GetAuditLogsResult AuditRepository::getLogsPaginated(int page, int pageSize, const std::string& operatorFilter) {
    GetAuditLogsResult res;
    res.current_page = page;

    int offset = (page - 1) * pageSize;
    if (offset < 0) offset = 0;
    bool hasFilter = !operatorFilter.empty();

    // 2a. Đếm tổng số bản ghi phục vụ tính tổng số trang (total_pages)
    std::string countSql = "SELECT COUNT(*) FROM audit_logs";
    nlohmann::json countArgs = nlohmann::json::array();
    if (hasFilter) {
        countSql += " WHERE operator_id = ?;";
        countArgs.push_back({{"type", "text"}, {"value", operatorFilter}});
    } else {
        countSql += ";";
    }

    nlohmann::json countRes = m_tursoDb.execute(countSql, countArgs);
    try {
        if (!countRes.contains("results") || !countRes["results"].is_array() || countRes["results"].empty() ||
            !countRes["results"][0].is_object() || countRes["results"][0].value("type", "") != "ok") {
            throw std::runtime_error("Không thể đếm nhật ký audit từ database.");
        }
        const auto& rows = countRes["results"][0]["response"]["result"]["rows"];
        if (!rows.is_array() || rows.empty() || !rows[0].is_array() || rows[0].empty()) {
            throw std::runtime_error("Database trả về tổng số nhật ký không hợp lệ.");
        }
        res.total_items = std::stoi(extractCell(rows[0][0]));
    } catch (const std::exception& error) {
        res.message = error.what();
        return res;
    }

    res.total_pages = (res.total_items + pageSize - 1) / pageSize;
    if (res.total_pages == 0) res.total_pages = 1;

    // 2b. Truy vấn dữ liệu phân trang theo thứ tự thời gian mới nhất
    std::string sql = "SELECT log_id, operator_id, action, target_id, details, created_at FROM audit_logs";
    nlohmann::json args = nlohmann::json::array();

    if (hasFilter) {
        sql += " WHERE operator_id = ? ORDER BY created_at DESC LIMIT ? OFFSET ?;";
        args.push_back({{"type", "text"}, {"value", operatorFilter}});
    } else {
        sql += " ORDER BY created_at DESC LIMIT ? OFFSET ?;";
    }

    args.push_back({{"type", "numeric"}, {"value", pageSize}});
    args.push_back({{"type", "numeric"}, {"value", offset}});

    nlohmann::json queryRes = m_tursoDb.execute(sql, args);
    try {
        if (!queryRes.contains("results") || !queryRes["results"].is_array() || queryRes["results"].empty() ||
            !queryRes["results"][0].is_object() || queryRes["results"][0].value("type", "") != "ok") {
            throw std::runtime_error("Không thể truy vấn danh sách nhật ký audit.");
        }
        const auto& rows = queryRes["results"][0]["response"]["result"]["rows"];
        if (!rows.is_array()) throw std::runtime_error("Database trả về danh sách nhật ký không hợp lệ.");
        for (const auto& row : rows) {
            if (!row.is_array() || row.size() < 6) {
                throw std::runtime_error("Bản ghi nhật ký audit không đầy đủ.");
            }
            res.items.push_back(AuditLogItemDTO{
                extractCell(row[0]), extractCell(row[1]), extractCell(row[2]),
                extractCell(row[3]), extractCell(row[4]), extractCell(row[5])
            });
        }
    } catch (const std::exception& error) {
        res.items.clear();
        res.message = error.what();
        return res;
    }

    res.success = true;
    res.message = "Tra cứu nhật ký Audit Log thành công!";
    return res;
}

} // namespace Banking
