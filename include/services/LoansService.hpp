#ifndef LOAN_SERVICE_HPP
#define LOAN_SERVICE_HPP

#include <string>
#include <ctime>
#include <vector>
#include <utility>

#include "dtos/LoanDTOs.hpp"
#include "repository/LoansRepository.hpp"
#include "repository/AccountRepository.hpp"
#include "repository/UserRepository.hpp"
#include "repository/AuditRepository.hpp"
#include "core/SessionManager.hpp"
#include "core/TursoDB.hpp"

namespace Banking {

class LoanService {
private:
    LoanRepository&    m_loanRepo;
    AccountRepository& m_accRepo;
    UserRepository&    m_userRepo;
    AuditRepository&   m_auditRepo;
    SessionManager&    m_sessionManager;
    TursoDB&           m_tursoDb;

    const int64_t TELLER_APPROVAL_LIMIT_VND = 100000000; // Hạn mức duyệt Teller <= 100 triệu

public:
    LoanService(LoanRepository& loanRepo, AccountRepository& accRepo,
                UserRepository& userRepo, AuditRepository& auditRepo,
                SessionManager& sessionManager, TursoDB& tursoDb);

    // Lấy danh sách khoản vay thuộc đúng phiên đăng nhập hiện tại.
    std::vector<LoanModel> getLoansForSession(const std::string& sessionToken);
    StaffLoanApplicationsResult getApplicationsForStaff(const std::string& sessionToken);
    json getAvailableTerms();

    // 1. TẠO ĐƠN XIN VAY
    LoanOperationResult applyLoan(const ApplyLoanRequest& req);

    // 2. PHÊ DUYỆT KHOẢN VAY (KÈM KIỂM TRA QUYỀN RBAC)
    LoanOperationResult approveLoan(const ApproveLoanRequest& req);

    // 3. GIẢI NGÂN KHOẢN VAY
    LoanOperationResult disburseLoan(const DisburseLoanRequest& req);
    LoanDocumentsResult getDocuments(const std::string& sessionToken, const std::string& loanId);
    LoanOperationResult submitDocument(const SubmitLoanDocumentRequest& request);
    LoanOperationResult reviewDocument(const ReviewLoanDocumentRequest& request);
    LoanDocumentLinkResult getDocumentLink(const LoanDocumentLinkRequest& request);
    LoanSchedulesResult getSchedules(const std::string& sessionToken, const std::string& loanId);
    PayInstallmentResult payInstallment(const PayInstallmentRequest& request);
};

} // namespace Banking

#endif // LOAN_SERVICE_HPP
