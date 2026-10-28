#ifndef BILL_SERVICE_HPP
#define BILL_SERVICE_HPP

#include "dtos/BillDTOs.hpp"
#include "repository/BillRepository.hpp"
#include "repository/UserRepository.hpp"
#include "repository/AccountRepository.hpp"
#include "repository/TransactionRepository.hpp"
#include "repository/AuditRepository.hpp"
#include "core/SessionManager.hpp"

namespace Banking {

class BillService {
private:
    BillRepository&        m_billRepo;
    UserRepository&        m_userRepo;
    AccountRepository&     m_accRepo;
    TransactionRepository& m_txnRepo;
    AuditRepository&       m_auditRepo;
    SessionManager&        m_sessionManager;

public:
    BillService(BillRepository& billRepo, UserRepository& userRepo,
                AccountRepository& accRepo, TransactionRepository& txnRepo,
                AuditRepository& auditRepo, SessionManager& sessionManager);

    GetBillProvidersResult getProviders(const std::string& sessionToken);
    QueryBillResult queryBill(const QueryBillRequest& req);
    PayBillResult payBill(const PayBillRequest& req);
};

} // namespace Banking
#endif // BILL_SERVICE_HPP