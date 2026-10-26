#ifndef LOAN_MODEL_HPP
#define LOAN_MODEL_HPP

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace Banking {

// =========================================================================
// 1. STRUCTURE: RepaymentScheduleItem
// =========================================================================
struct RepaymentScheduleItem {
    int     period = 0;           // Kỳ thứ (1, 2, ..., N tháng)
    int64_t principalVnd = 0;     // Tiền gốc phải trả trong kỳ (VNĐ)
    int64_t interestVnd = 0;      // Tiền lãi phải trả trong kỳ (VNĐ)
    int64_t totalPaymentVnd = 0;  // Tổng số tiền trả trong kỳ (Gốc + Lãi)
    int64_t remainingDebtVnd = 0; // Dư nợ gốc còn lại sau khi trả kỳ này
};

// =========================================================================
// 2. DOMAIN MODEL: LoanModel
// =========================================================================
class LoanModel {
private:
    std::string m_loanId;
    std::string m_userId;
    int64_t     m_originalAmountVnd = 0;   // Số tiền vay gốc ban đầu
    int64_t     m_remainingDebtVnd = 0;    // Dư nợ gốc còn lại
    double      m_annualInterestRate = 0.0; // Lãi suất (%/năm)
    int         m_termMonths = 0;          // Kỳ hạn vay (tháng)
    std::string m_calcMethod;              // "FLAT" hoặc "DECREASING"
    std::string m_collateralInfo;          // Thông tin tài sản đảm bảo
    std::string m_status;                  // "PENDING", "APPROVED", "DISBURSED", "REJECTED", "CLOSED"
    std::string m_createdAt;               // Thời gian tạo hồ sơ
    std::string m_approvedBy;              // ID cán bộ phê duyệt
    std::string m_disbursedAccountId;      // Tài khoản thanh toán nhận tiền giải ngân
    std::string m_purpose;
    int64_t m_monthlyIncomeVnd = 0;

public:
    LoanModel() = default;

    LoanModel(std::string loanId, std::string userId, int64_t originalAmountVnd,
              int64_t remainingDebtVnd, double annualInterestRate, int termMonths,
              std::string calcMethod, std::string collateralInfo, std::string status,
              std::string createdAt, std::string approvedBy = "", std::string disbursedAccountId = "",
              std::string purpose = "", int64_t monthlyIncomeVnd = 0);

    // Status Checkers
    [[nodiscard]] bool isPending() const;
    [[nodiscard]] bool isApproved() const;
    [[nodiscard]] bool isDisbursed() const;
    [[nodiscard]] bool isClosed() const;
    [[nodiscard]] bool isRejected() const;

    // Financial Calculations
    [[nodiscard]] int64_t calculateMonthlyPrincipal() const;
    [[nodiscard]] int64_t calculateInterestForAmount(int64_t debtAmount) const;
    [[nodiscard]] int64_t calculateMonthlyInstallment(int period = 1) const;
    [[nodiscard]] std::vector<RepaymentScheduleItem> generateRepaymentSchedule() const;
    bool repay(int64_t repayAmountVnd);

    // Getters & Setters
    [[nodiscard]] const std::string& getLoanId() const;
    [[nodiscard]] const std::string& getUserId() const;
    [[nodiscard]] int64_t getOriginalAmountVnd() const;
    [[nodiscard]] int64_t getRemainingDebtVnd() const;
    [[nodiscard]] double getAnnualInterestRate() const;
    [[nodiscard]] int getTermMonths() const;
    [[nodiscard]] const std::string& getCalcMethod() const;
    [[nodiscard]] const std::string& getCollateralInfo() const;
    [[nodiscard]] const std::string& getStatus() const;
    [[nodiscard]] const std::string& getCreatedAt() const;
    [[nodiscard]] const std::string& getApprovedBy() const;
    [[nodiscard]] const std::string& getDisbursedAccountId() const;
    [[nodiscard]] const std::string& getPurpose() const;
    [[nodiscard]] int64_t getMonthlyIncomeVnd() const;

    void setStatus(std::string status);
    void setApprovedBy(std::string operatorId);
    void setDisbursedAccountId(std::string accId);
};

// =========================================================================
// 3. AGGREGATE MODEL: LoanBookModel
// =========================================================================
class LoanBookModel {
private:
    std::string m_userId;
    std::vector<LoanModel> m_loans;

public:
    LoanBookModel() = default;
    explicit LoanBookModel(std::string userId);
    LoanBookModel(std::string userId, std::vector<LoanModel> loans);

    void addLoan(const LoanModel& loan);

    [[nodiscard]] std::optional<LoanModel> findById(const std::string& loanId) const;
    [[nodiscard]] std::vector<LoanModel> getDisbursedLoans() const;
    [[nodiscard]] int64_t getTotalRemainingDebtVnd() const;
    [[nodiscard]] int64_t getTotalNextMonthInstallmentVnd() const;

    [[nodiscard]] const std::string& getUserId() const;
    [[nodiscard]] const std::vector<LoanModel>& getLoans() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;
};

} // namespace Banking

#endif // LOAN_MODEL_HPP
