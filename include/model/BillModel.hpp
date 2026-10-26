#ifndef BILL_MODEL_HPP
#define BILL_MODEL_HPP

#include <string>
#include <cstdint>

namespace Banking {

struct BillProviderModel {
    std::string providerId;
    std::string providerName;
    std::string serviceType;
};

class BillModel {
private:
    std::string m_billId;
    std::string m_providerId;
    std::string m_providerName;
    std::string m_customerCode;
    std::string m_customerName;
    int64_t     m_amountVnd = 0;   // Tiền cước (VNĐ)
    std::string m_serviceType;    // "ELECTRICITY", "WATER", "INTERNET", "TUITION"
    std::string m_status;         // "UNPAID", "PAID", "EXPIRED"
    std::string m_dueDate;
    std::string m_period;         // Ví dụ: "10/2026"

public:
    BillModel() = default;

    BillModel(std::string billId, std::string providerId, std::string providerName,
              std::string customerCode, std::string customerName, int64_t amountVnd,
              std::string serviceType, std::string status, std::string dueDate, std::string period);

    [[nodiscard]] bool isPayable() const;
    void markAsPaid();

    [[nodiscard]] const std::string& getBillId() const;
    [[nodiscard]] const std::string& getProviderId() const;
    [[nodiscard]] const std::string& getProviderName() const;
    [[nodiscard]] const std::string& getCustomerCode() const;
    [[nodiscard]] const std::string& getCustomerName() const;
    [[nodiscard]] int64_t getAmountVnd() const;
    [[nodiscard]] const std::string& getServiceType() const;
    [[nodiscard]] const std::string& getStatus() const;
    [[nodiscard]] const std::string& getDueDate() const;
    [[nodiscard]] const std::string& getPeriod() const;
};

} // namespace Banking

#endif // BILL_MODEL_HPP