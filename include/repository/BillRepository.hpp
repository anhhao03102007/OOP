#ifndef BILL_REPOSITORY_HPP
#define BILL_REPOSITORY_HPP

#include <optional>
#include <string>
#include <vector>
#include <core/json.hpp>
#include "core/TursoDB.hpp"
#include "model/BillModel.hpp"

namespace Banking {

using json = nlohmann::json;

class BillRepository {
private:
    TursoDB& m_tursoDb;

    static std::string extractCell(const json& cell);

public:
    explicit BillRepository(TursoDB& tursoDb);

    // 1. Tra cứu hóa đơn chưa thanh toán theo nhà cung cấp và mã khách hàng
    std::optional<BillModel> findUnpaidBill(const std::string& providerId, const std::string& customerCode);

    // 2. Tra cứu hóa đơn theo Mã hóa đơn
    std::optional<BillModel> findById(const std::string& billId);

    // Danh sách nhà cung cấp có hóa đơn trong database
    std::vector<BillProviderModel> findProviders();
    bool createSimulatedBill(const std::string& billId, const std::string& providerId, const std::string& providerName,
                             const std::string& customerCode, const std::string& customerName, int64_t amountVnd,
                             const std::string& serviceType, const std::string& period);

    // 3. Cập nhật trạng thái hóa đơn sang "PAID"
    bool markAsPaid(const std::string& billId);
};

} // namespace Banking

#endif // BILL_REPOSITORY_HPP
