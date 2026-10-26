#ifndef CREDIT_SERVICE_HPP
#define CREDIT_SERVICE_HPP
#include <core/TursoDB.hpp>
#include <core/SessionManager.hpp>
#include <repository/UserRepository.hpp>
#include <dtos/CreditDTOs.hpp>
#include <dtos/TransactionDTOs.hpp>

namespace Banking {
class CreditService {
    TursoDB& db;
    UserRepository& users;
    SessionManager& sessions;
    bool ensureSchema() const;
    bool accrueInterest(const std::string& accountId, const std::string& userId) const;
public:
    CreditService(TursoDB& database, UserRepository& userRepository, SessionManager& sessionManager);
    CreditAccountDTO getMyAccount(const std::string& sessionToken) const;
    CreditApplicationsResult getApplications(const CreditApplicationsRequest& request) const;
    CreditApplicationsResult apply(const CreditApplicationRequest& request) const;
    CreditDocumentResult submitDocument(const CreditDocumentRequest& request) const;
    CreditDocumentResult reviewDocument(const ReviewCreditDocumentRequest& request) const;
    CreditApplicationsResult decide(const CreditDecisionRequest& request) const;
    TransactionResult spend(const CreditActionRequest& request) const;
    TransactionResult repay(const CreditActionRequest& request) const;
};
}
#endif
