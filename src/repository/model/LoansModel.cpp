#include "model/LoansModel.hpp"
#include <cmath>
#include <utility>

namespace Banking {

// =========================================================================
// 1. LOAN MODEL IMPLEMENTATION
// =========================================================================
LoanModel::LoanModel(std::string loanId, std::string userId, int64_t originalAmountVnd,
                     int64_t remainingDebtVnd, double annualInterestRate, int termMonths,
                     std::string calcMethod, std::string collateralInfo, std::string status,
                     std::string createdAt, std::string approvedBy, std::string disbursedAccountId,
                     std::string purpose, int64_t monthlyIncomeVnd)
    : m_loanId(std::move(loanId)), m_userId(std::move(userId)),
      m_originalAmountVnd(originalAmountVnd), m_remainingDebtVnd(remainingDebtVnd),
      m_annualInterestRate(annualInterestRate), m_termMonths(termMonths),
      m_calcMethod(std::move(calcMethod)), m_collateralInfo(std::move(collateralInfo)),
      m_status(std::move(status)), m_createdAt(std::move(createdAt)),
      m_approvedBy(std::move(approvedBy)), m_disbursedAccountId(std::move(disbursedAccountId)),
      m_purpose(std::move(purpose)), m_monthlyIncomeVnd(monthlyIncomeVnd) {}

bool LoanModel::isPending() const { return m_status == "PENDING"; }
bool LoanModel::isApproved() const { return m_status == "APPROVED"; }
bool LoanModel::isDisbursed() const { return m_status == "DISBURSED"; }
bool LoanModel::isClosed() const { return m_status == "CLOSED"; }
bool LoanModel::isRejected() const { return m_status == "REJECTED"; }

int64_t LoanModel::calculateMonthlyPrincipal() const {
    if (m_termMonths <= 0) return 0;
    return m_originalAmountVnd / m_termMonths;
}

int64_t LoanModel::calculateInterestForAmount(int64_t debtAmount) const {
    if (debtAmount <= 0 || m_annualInterestRate <= 0.0) return 0;
    double monthlyRate = (m_annualInterestRate / 100.0) / 12.0;
    return static_cast<int64_t>(std::round(debtAmount * monthlyRate));
}

int64_t LoanModel::calculateMonthlyInstallment(int period) const {
    if (m_termMonths <= 0 || m_originalAmountVnd <= 0) return 0;

    int64_t monthlyPrincipal = calculateMonthlyPrincipal();

    if (m_calcMethod == "FLAT") {
        int64_t monthlyInterest = calculateInterestForAmount(m_originalAmountVnd);
        return monthlyPrincipal + monthlyInterest;
    } else {
        int64_t debtAtPeriod = m_originalAmountVnd - (monthlyPrincipal * (period - 1));
        if (debtAtPeriod < 0) debtAtPeriod = 0;
        int64_t monthlyInterest = calculateInterestForAmount(debtAtPeriod);
        return monthlyPrincipal + monthlyInterest;
    }
}

std::vector<RepaymentScheduleItem> LoanModel::generateRepaymentSchedule() const {
    std::vector<RepaymentScheduleItem> schedule;
    if (m_termMonths <= 0 || m_originalAmountVnd <= 0) return schedule;

    int64_t monthlyPrincipal = calculateMonthlyPrincipal();
    int64_t currentDebt = m_originalAmountVnd;

    for (int period = 1; period <= m_termMonths; ++period) {
        int64_t interest = 0;

        if (m_calcMethod == "FLAT") {
            interest = calculateInterestForAmount(m_originalAmountVnd);
        } else {
            interest = calculateInterestForAmount(currentDebt);
        }

        int64_t principal = (period == m_termMonths) ? currentDebt : monthlyPrincipal;
        int64_t totalPayment = principal + interest;

        currentDebt -= principal;
        if (currentDebt < 0) currentDebt = 0;

        schedule.push_back(RepaymentScheduleItem{
            period,
            principal,
            interest,
            totalPayment,
            currentDebt
        });
    }

    return schedule;
}

bool LoanModel::repay(int64_t repayAmountVnd) {
    if (!isDisbursed() || repayAmountVnd <= 0) return false;

    m_remainingDebtVnd -= repayAmountVnd;
    if (m_remainingDebtVnd <= 0) {
        m_remainingDebtVnd = 0;
        m_status = "CLOSED";
    }
    return true;
}

const std::string& LoanModel::getLoanId() const { return m_loanId; }
const std::string& LoanModel::getUserId() const { return m_userId; }
int64_t LoanModel::getOriginalAmountVnd() const { return m_originalAmountVnd; }
int64_t LoanModel::getRemainingDebtVnd() const { return m_remainingDebtVnd; }
double LoanModel::getAnnualInterestRate() const { return m_annualInterestRate; }
int LoanModel::getTermMonths() const { return m_termMonths; }
const std::string& LoanModel::getCalcMethod() const { return m_calcMethod; }
const std::string& LoanModel::getCollateralInfo() const { return m_collateralInfo; }
const std::string& LoanModel::getStatus() const { return m_status; }
const std::string& LoanModel::getCreatedAt() const { return m_createdAt; }
const std::string& LoanModel::getApprovedBy() const { return m_approvedBy; }
const std::string& LoanModel::getDisbursedAccountId() const { return m_disbursedAccountId; }
const std::string& LoanModel::getPurpose() const { return m_purpose; }
int64_t LoanModel::getMonthlyIncomeVnd() const { return m_monthlyIncomeVnd; }

void LoanModel::setStatus(std::string status) { m_status = std::move(status); }
void LoanModel::setApprovedBy(std::string operatorId) { m_approvedBy = std::move(operatorId); }
void LoanModel::setDisbursedAccountId(std::string accId) { m_disbursedAccountId = std::move(accId); }

// =========================================================================
// 2. LOAN BOOK MODEL IMPLEMENTATION
// =========================================================================
LoanBookModel::LoanBookModel(std::string userId) : m_userId(std::move(userId)) {}

LoanBookModel::LoanBookModel(std::string userId, std::vector<LoanModel> loans)
    : m_userId(std::move(userId)), m_loans(std::move(loans)) {}

void LoanBookModel::addLoan(const LoanModel& loan) {
    m_loans.push_back(loan);
}

std::optional<LoanModel> LoanBookModel::findById(const std::string& loanId) const {
    for (const auto& loan : m_loans) {
        if (loan.getLoanId() == loanId) return loan;
    }
    return std::nullopt;
}

std::vector<LoanModel> LoanBookModel::getDisbursedLoans() const {
    std::vector<LoanModel> list;
    for (const auto& loan : m_loans) {
        if (loan.isDisbursed()) list.push_back(loan);
    }
    return list;
}

int64_t LoanBookModel::getTotalRemainingDebtVnd() const {
    int64_t total = 0;
    for (const auto& loan : m_loans) {
        if (loan.isDisbursed()) {
            total += loan.getRemainingDebtVnd();
        }
    }
    return total;
}

int64_t LoanBookModel::getTotalNextMonthInstallmentVnd() const {
    int64_t total = 0;
    for (const auto& loan : m_loans) {
        if (loan.isDisbursed()) {
            total += loan.calculateMonthlyInstallment(1);
        }
    }
    return total;
}

const std::string& LoanBookModel::getUserId() const { return m_userId; }
const std::vector<LoanModel>& LoanBookModel::getLoans() const { return m_loans; }
size_t LoanBookModel::size() const { return m_loans.size(); }
bool LoanBookModel::empty() const { return m_loans.empty(); }

} // namespace Banking
