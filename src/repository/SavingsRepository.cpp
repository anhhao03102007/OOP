#include "repository/SavingsRepository.hpp"
#include <stdexcept>

namespace Banking {

SavingsRepository::SavingsRepository(TursoDB& tursoDb) : m_tursoDb(tursoDb) {}

std::string SavingsRepository::extractCell(const json& cell) {
    if (cell.is_object() && cell.contains("value") && !cell["value"].is_null()) {
        const auto& value = cell["value"];
        return value.is_string() ? value.get<std::string>() : value.dump();
    }
    return "";
}

TermSavingsAccount SavingsRepository::mapRowToModel(const json& r) {
    return TermSavingsAccount(
        extractCell(r[0]),                             // accountId (savings_id)
        extractCell(r[1]),                             // userId
        std::stoll(extractCell(r[3])),                 // principalVnd
        std::stod(extractCell(r[4])),                  // rate
        std::stoi(extractCell(r[5])),                  // termMonths
        extractCell(r[9]),                             // maturityDate
        extractCell(r[6]),                             // option
        extractCell(r[7]),                             // status
        extractCell(r[10]),                            // createdAt
        std::stoi(extractCell(r[11]))                  // termDays
    );
}

// 1. Tìm sổ tiết kiệm theo Mã sổ (savings_id)
std::optional<TermSavingsAccount> SavingsRepository::findById(const std::string& savingsId) {
    std::string sql = 
        "SELECT savings_id, user_id, source_account_id, principal_vnd, interest_rate, "
        "term_months, maturity_option, status, start_date, maturity_date, created_at, term_days "
        "FROM savings_accounts WHERE savings_id = ? LIMIT 1;";

    json args = json::array({{{"type", "text"}, {"value", savingsId}}});
    json res = m_tursoDb.execute(sql, args);

    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                return mapRowToModel(rows[0]);
            }
        }
    } catch (...) {}
    return std::nullopt;
}

// 2. Lấy tất cả sổ tiết kiệm thuộc về một Khách hàng
SavingsBook SavingsRepository::findSavingsBookByUserId(const std::string& userId, const std::string& statusFilter) {
    SavingsBook book(userId);
    std::string sql = 
        "SELECT savings_id, user_id, source_account_id, principal_vnd, interest_rate, "
        "term_months, maturity_option, status, start_date, maturity_date, created_at, term_days "
        "FROM savings_accounts WHERE user_id = ?";

    json args = json::array({{{"type", "text"}, {"value", userId}}});

    if (!statusFilter.empty() && statusFilter != "ALL") {
        sql += " AND status = ?";
        args.push_back({{"type", "text"}, {"value", statusFilter}});
    }

    sql += " ORDER BY created_at DESC;";

    json res = m_tursoDb.execute(sql, args);
    if (!res.contains("results") || !res["results"].is_array() || res["results"].empty() ||
        !res["results"][0].is_object() || res["results"][0].value("type", "") != "ok") {
        throw std::runtime_error("Không thể tải danh sách sổ tiết kiệm từ database.");
    }
    const auto& rows = res["results"][0]["response"]["result"]["rows"];
    if (!rows.is_array()) throw std::runtime_error("Database trả về danh sách sổ tiết kiệm không hợp lệ.");
    for (const auto& row : rows) {
        if (!row.is_array() || row.size() < 12) {
            throw std::runtime_error("Bản ghi sổ tiết kiệm không đầy đủ.");
        }
        book.addAccount(mapRowToModel(row));
    }

    return book;
}

// 3. Cập nhật trạng thái sổ tiết kiệm
bool SavingsRepository::updateStatus(const std::string& savingsId, const std::string& newStatus) {
    std::string sql = "UPDATE savings_accounts SET status = ? WHERE savings_id = ?;";
    json args = json::array({
        {{"type", "text"}, {"value", newStatus}},
        {{"type", "text"}, {"value", savingsId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() &&
           res["results"][0].value("type", "") == "ok";
}

// 4. Cập nhật phương thức đáo hạn (Maturity Option)
bool SavingsRepository::updateMaturityOption(const std::string& savingsId, const std::string& newOption) {
    std::string sql = "UPDATE savings_accounts SET maturity_option = ? WHERE savings_id = ? AND status = 'ACTIVE';";
    json args = json::array({
        {{"type", "text"}, {"value", newOption}},
        {{"type", "text"}, {"value", savingsId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() &&
           res["results"][0].value("type", "") == "ok";
}

// 5. Đếm tổng số sổ tiết kiệm đang hoạt động của một khách hàng
int SavingsRepository::countActiveSavings(const std::string& userId) {
    std::string sql = "SELECT COUNT(*) FROM savings_accounts WHERE user_id = ? AND status = 'ACTIVE';";
    json args = json::array({{{"type", "text"}, {"value", userId}}});

    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                return std::stoi(extractCell(rows[0][0]));
            }
        }
    } catch (...) {}
    return 0;
}

} // namespace Banking
