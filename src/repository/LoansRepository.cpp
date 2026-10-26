#include "repository/LoansRepository.hpp"
#include <stdexcept>

namespace Banking {

LoanRepository::LoanRepository(TursoDB& tursoDb) : m_tursoDb(tursoDb) {}

std::string LoanRepository::extractCell(const json& cell) {
    if (cell.is_object() && cell.contains("value") && !cell["value"].is_null()) {
        const auto& value = cell["value"];
        return value.is_string() ? value.get<std::string>() : value.dump();
    }
    return "";
}

// 1. Tra cứu khoản vay theo Loan ID
std::optional<LoanModel> LoanRepository::findById(const std::string& loanId) {
    std::string sql = 
        "SELECT loan_id, user_id, original_amount_vnd, remaining_debt_vnd, annual_interest_rate, "
        "term_months, calc_method, collateral_info, status, created_at, approved_by, disbursed_account_id, purpose, monthly_income_vnd "
        "FROM loans WHERE loan_id = ? LIMIT 1;";

    json args = json::array({{{"type", "text"}, {"value", loanId}}});
    json res = m_tursoDb.execute(sql, args);

    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                auto r = rows[0];
                return LoanModel(
                    extractCell(r[0]),                                 // loanId
                    extractCell(r[1]),                                 // userId
                    std::stoll(extractCell(r[2])),                     // originalAmountVnd
                    std::stoll(extractCell(r[3])),                     // remainingDebtVnd
                    std::stod(extractCell(r[4])),                      // annualInterestRate
                    std::stoi(extractCell(r[5])),                      // termMonths
                    extractCell(r[6]),                                 // calcMethod
                    extractCell(r[7]),                                 // collateralInfo
                    extractCell(r[8]),                                 // status
                    extractCell(r[9]),                                 // createdAt
                    extractCell(r[10]),                                // approvedBy
                    extractCell(r[11]),                                // disbursedAccountId
                    extractCell(r[12]), std::stoll(extractCell(r[13]))
                );
            }
        }
    } catch (...) {}
    return std::nullopt;
}

// 2. Tra cứu toàn bộ danh sách khoản vay của một User
std::vector<LoanModel> LoanRepository::findByUserId(const std::string& userId) {
    std::vector<LoanModel> list;
    std::string sql = 
        "SELECT loan_id, user_id, original_amount_vnd, remaining_debt_vnd, annual_interest_rate, "
        "term_months, calc_method, collateral_info, status, created_at, approved_by, disbursed_account_id, purpose, monthly_income_vnd "
        "FROM loans WHERE user_id = ? ORDER BY created_at DESC;";

    json args = json::array({{{"type", "text"}, {"value", userId}}});
    json res = m_tursoDb.execute(sql, args);

    if (!res.contains("results") || !res["results"].is_array() || res["results"].empty() ||
        !res["results"][0].is_object() || res["results"][0].value("type", "") != "ok") {
        throw std::runtime_error("Không thể tải khoản vay của khách hàng từ database.");
    }
    const auto& rows = res["results"][0]["response"]["result"]["rows"];
    if (!rows.is_array()) throw std::runtime_error("Database trả về danh sách khoản vay không hợp lệ.");
    for (const auto& row : rows) {
        if (!row.is_array() || row.size() < 14) {
            throw std::runtime_error("Bản ghi khoản vay không đầy đủ.");
        }
        list.push_back(LoanModel(
            extractCell(row[0]), extractCell(row[1]),
            std::stoll(extractCell(row[2])), std::stoll(extractCell(row[3])),
            std::stod(extractCell(row[4])), std::stoi(extractCell(row[5])),
            extractCell(row[6]), extractCell(row[7]), extractCell(row[8]),
            extractCell(row[9]), extractCell(row[10]), extractCell(row[11]),
            extractCell(row[12]), std::stoll(extractCell(row[13]))
        ));
    }
    return list;
}

// Lấy toàn bộ khoản vay phục vụ màn hình Teller/Admin.
std::vector<LoanModel> LoanRepository::findAll() {
    std::vector<LoanModel> list;
    const std::string sql =
        "SELECT loan_id, user_id, original_amount_vnd, remaining_debt_vnd, annual_interest_rate, "
        "term_months, calc_method, collateral_info, status, created_at, approved_by, disbursed_account_id, purpose, monthly_income_vnd "
        "FROM loans ORDER BY created_at DESC;";
    const json res = m_tursoDb.execute(sql);
    if (!res.contains("results") || !res["results"].is_array() || res["results"].empty() ||
        !res["results"][0].is_object() || res["results"][0].value("type", "") != "ok") {
        throw std::runtime_error("Không thể tải hồ sơ vay từ database.");
    }
    const auto& rows = res["results"][0]["response"]["result"]["rows"];
    if (!rows.is_array()) throw std::runtime_error("Database trả về danh sách hồ sơ vay không hợp lệ.");
    for (const auto& row : rows) {
        if (!row.is_array() || row.size() < 14) {
            throw std::runtime_error("Bản ghi hồ sơ vay không đầy đủ.");
        }
        list.emplace_back(extractCell(row[0]), extractCell(row[1]),
            std::stoll(extractCell(row[2])), std::stoll(extractCell(row[3])),
            std::stod(extractCell(row[4])), std::stoi(extractCell(row[5])),
            extractCell(row[6]), extractCell(row[7]), extractCell(row[8]),
            extractCell(row[9]), extractCell(row[10]), extractCell(row[11]),
            extractCell(row[12]), std::stoll(extractCell(row[13])));
    }
    return list;
}

// 3. Cập nhật trạng thái và thông tin phê duyệt khoản vay
bool LoanRepository::updateLoanStatus(const std::string& loanId, const std::string& status, 
                                     const std::string& approvedBy, const std::string& disbursedAccountId) {
    std::string sql = 
        "UPDATE loans SET status = ?, approved_by = ?, disbursed_account_id = ? WHERE loan_id = ?;";
    
    json args = json::array({
        {{"type", "text"}, {"value", status}},
        {{"type", "text"}, {"value", approvedBy}},
        {{"type", "text"}, {"value", disbursedAccountId}},
        {{"type", "text"}, {"value", loanId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

// 4. Cập nhật số dư nợ còn lại (Dùng cho gạch nợ / trả góp)
bool LoanRepository::updateRemainingDebt(const std::string& loanId, int64_t newRemainingDebtVnd) {
    std::string sql = "UPDATE loans SET remaining_debt_vnd = ? WHERE loan_id = ?;";
    
    json args = json::array({
        {{"type", "numeric"}, {"value", newRemainingDebtVnd}},
        {{"type", "text"},    {"value", loanId}}
    });

    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

} // namespace Banking
