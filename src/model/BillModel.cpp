#include "model/BillModel.hpp"
#include <utility>

namespace Banking {

BillModel::BillModel(std::string billId, std::string providerId, std::string providerName,
                     std::string customerCode, std::string customerName, int64_t amountVnd,
                     std::string serviceType, std::string status, std::string dueDate, std::string period)
    : m_billId(std::move(billId)), m_providerId(std::move(providerId)),
      m_providerName(std::move(providerName)), m_customerCode(std::move(customerCode)),
      m_customerName(std::move(customerName)), m_amountVnd(amountVnd),
      m_serviceType(std::move(serviceType)), m_status(std::move(status)),
      m_dueDate(std::move(dueDate)), m_period(std::move(period)) {}

bool BillModel::isPayable() const {
    return m_status == "UNPAID" && m_amountVnd > 0;
}

void BillModel::markAsPaid() { 
    m_status = "PAID"; 
}

const std::string& BillModel::getBillId() const { return m_billId; }
const std::string& BillModel::getProviderId() const { return m_providerId; }
const std::string& BillModel::getProviderName() const { return m_providerName; }
const std::string& BillModel::getCustomerCode() const { return m_customerCode; }
const std::string& BillModel::getCustomerName() const { return m_customerName; }
int64_t BillModel::getAmountVnd() const { return m_amountVnd; }
const std::string& BillModel::getServiceType() const { return m_serviceType; }
const std::string& BillModel::getStatus() const { return m_status; }
const std::string& BillModel::getDueDate() const { return m_dueDate; }
const std::string& BillModel::getPeriod() const { return m_period; }

} // namespace Banking