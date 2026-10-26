#include "repository/BillRepository.hpp"

namespace Banking {

BillRepository::BillRepository(TursoDB& tursoDb) : m_tursoDb(tursoDb) {}

std::string BillRepository::extractCell(const json& cell) {
    if (cell.is_object() && cell.contains("value") && !cell["value"].is_null()) {
        const auto& value = cell["value"];
        return value.is_string() ? value.get<std::string>() : value.dump();
    }
    return "";
}

// 1. Tra cứu hóa đơn chưa thanh toán theo nhà cung cấp và mã khách hàng
std::optional<BillModel> BillRepository::findUnpaidBill(const std::string& providerId, const std::string& customerCode) {
    std::string sql = 
        "SELECT bill_id, provider_id, provider_name, customer_code, customer_name, "
        "amount_vnd, service_type, status, due_date, period "
        "FROM bills WHERE provider_id = ? AND customer_code = ? AND status = 'UNPAID' LIMIT 1;";

    json args = json::array({
        {{"type", "text"}, {"value", providerId}},
        {{"type", "text"}, {"value", customerCode}}
    });

    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                auto r = rows[0];
                return BillModel(
                    extractCell(r[0]), extractCell(r[1]), extractCell(r[2]),
                    extractCell(r[3]), extractCell(r[4]), std::stoll(extractCell(r[5])),
                    extractCell(r[6]), extractCell(r[7]), extractCell(r[8]), extractCell(r[9])
                );
            }
        }
    } catch (...) {}
    return std::nullopt;
}

// 2. Tra cứu hóa đơn theo Mã hóa đơn
std::optional<BillModel> BillRepository::findById(const std::string& billId) {
    std::string sql = 
        "SELECT bill_id, provider_id, provider_name, customer_code, customer_name, "
        "amount_vnd, service_type, status, due_date, period "
        "FROM bills WHERE bill_id = ? LIMIT 1;";

    json args = json::array({{{"type", "text"}, {"value", billId}}});
    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                auto r = rows[0];
                return BillModel(
                    extractCell(r[0]), extractCell(r[1]), extractCell(r[2]),
                    extractCell(r[3]), extractCell(r[4]), std::stoll(extractCell(r[5])),
                    extractCell(r[6]), extractCell(r[7]), extractCell(r[8]), extractCell(r[9])
                );
            }
        }
    } catch (...) {}
    return std::nullopt;
}

// Danh sách nhà cung cấp lấy từ hóa đơn đã cấu hình trong database.
std::vector<BillProviderModel> BillRepository::findProviders() {
    std::vector<BillProviderModel> providers;
    const std::string sql =
        "SELECT DISTINCT provider_id, provider_name, service_type FROM bills "
        "ORDER BY service_type, provider_name;";
    const json res = m_tursoDb.execute(sql);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            const auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array()) {
                for (const auto& row : rows) {
                    providers.push_back({extractCell(row[0]), extractCell(row[1]), extractCell(row[2])});
                }
            }
        }
    } catch (...) {}
    return providers;
}

bool BillRepository::createSimulatedBill(const std::string& billId, const std::string& providerId, const std::string& providerName,
                                          const std::string& customerCode, const std::string& customerName, int64_t amountVnd,
                                          const std::string& serviceType, const std::string& period) {
    const auto response = m_tursoDb.execute(
        "INSERT INTO bills (bill_id,provider_id,provider_name,customer_code,customer_name,amount_vnd,service_type,status,due_date,period) "
        "VALUES (?,?,?,?,?,?,?,'UNPAID',date('now','+30 day'),?) ON CONFLICT(bill_id) DO NOTHING;",
        json::array({{{"type","text"},{"value",billId}},{{"type","text"},{"value",providerId}},{{"type","text"},{"value",providerName}},
                     {{"type","text"},{"value",customerCode}},{{"type","text"},{"value",customerName}},{{"type","numeric"},{"value",amountVnd}},
                     {{"type","text"},{"value",serviceType}},{{"type","text"},{"value",period}}}));
    return response.contains("results") && !response["results"].empty() && response["results"][0].value("type", "") == "ok";
}

// 3. Cập nhật trạng thái hóa đơn sang "PAID"
bool BillRepository::markAsPaid(const std::string& billId) {
    std::string sql = "UPDATE bills SET status = 'PAID' WHERE bill_id = ? AND status = 'UNPAID';";
    json args = json::array({{{"type", "text"}, {"value", billId}}});
    json res = m_tursoDb.execute(sql, args);
    return res.contains("results") && !res["results"].empty() && res["results"][0].value("type", "") == "ok";
}

} // namespace Banking
