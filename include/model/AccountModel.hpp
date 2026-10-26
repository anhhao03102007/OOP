#ifndef ACCOUNT_MODEL_HPP
#define ACCOUNT_MODEL_HPP

#include <string>
#include <cstdint>

namespace Banking {

// =========================================================================
// 1. BASE CLASS: Account (Trừu tượng)
// =========================================================================
class Account {
protected:
    std::string m_accountId;
    std::string m_userId;
    int64_t     m_balanceVnd = 0;
    std::string m_status; // "ACTIVE", "BLOCKED", "CLOSED"
    std::string m_createdAt;

public:
    Account(std::string accountId, std::string userId, int64_t balanceVnd,
            std::string status, std::string createdAt);

    virtual ~Account() = default;

    // Polymorphic Member Functions
    [[nodiscard]] virtual bool canWithdraw(int64_t amountVnd) const = 0;
    virtual bool withdraw(int64_t amountVnd) = 0;
    virtual bool deposit(int64_t amountVnd) = 0;
    [[nodiscard]] virtual std::string getAccountType() const = 0;

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] const std::string& getAccountId() const;
    [[nodiscard]] const std::string& getUserId() const;
    [[nodiscard]] int64_t getBalanceVnd() const;
};

// =========================================================================
// 2. CHECKING ACCOUNT (Tài khoản Thanh toán / Thấu chi)
// =========================================================================
class CheckingAccount : public Account {
private:
    int64_t m_overdraftLimitVnd = 0; // Hạn mức thấu chi cho phép số dư âm

public:
    CheckingAccount(std::string accountId, std::string userId, int64_t balanceVnd,
                    std::string status, std::string createdAt, int64_t overdraftLimit = 0);

    [[nodiscard]] bool canWithdraw(int64_t amountVnd) const override;
    bool withdraw(int64_t amountVnd) override;
    bool deposit(int64_t amountVnd) override;
    [[nodiscard]] std::string getAccountType() const override;
};

// =========================================================================
// 3. DEMAND SAVINGS ACCOUNT (Tiết kiệm Không kỳ hạn)
// =========================================================================
class DemandSavingsAccount : public Account {
private:
    double m_annualRate = 0.1; // Lãi suất không kỳ hạn cố định 0.1%/năm

public:
    DemandSavingsAccount(std::string accountId, std::string userId, int64_t balanceVnd,
                         std::string status, std::string createdAt);

    [[nodiscard]] bool canWithdraw(int64_t amountVnd) const override;
    bool withdraw(int64_t amountVnd) override;
    bool deposit(int64_t amountVnd) override;
    [[nodiscard]] int64_t calculateDailyInterest() const;
    [[nodiscard]] std::string getAccountType() const override;
};

// =========================================================================
// 4. TERM SAVINGS ACCOUNT (Tiết kiệm Có kỳ hạn)
// =========================================================================
class TermSavingsAccount : public Account {
private:
    double      m_interestRate = 0.0;
    int         m_termMonths = 0;
    int         m_termDays = 0;
    std::string m_maturityDate;
    std::string m_maturityOption; // "RENEW_ALL", "CLOSE_TO_ACCOUNT"

public:
    TermSavingsAccount(std::string accountId, std::string userId, int64_t principalVnd,
                       double rate, int termMonths, std::string maturityDate,
                       std::string option, std::string status, std::string createdAt,
                       int termDays = 0);

    [[nodiscard]] bool canWithdraw(int64_t amountVnd) const override;
    bool withdraw(int64_t amountVnd) override;
    bool deposit(int64_t amountVnd) override;
    [[nodiscard]] int64_t calculateMaturityInterest() const;
    [[nodiscard]] int64_t calculateEarlyWithdrawalInterest(int actualDaysHeld) const;
    [[nodiscard]] std::string getAccountType() const override;
    [[nodiscard]] int64_t getOriginalAmountVnd() const;
    [[nodiscard]] double getInterestRate() const;
    [[nodiscard]] int getTermMonths() const;
    [[nodiscard]] int getTermDays() const;
    [[nodiscard]] const std::string& getMaturityDate() const;
    [[nodiscard]] const std::string& getMaturityOption() const;
    [[nodiscard]] const std::string& getStatus() const;
};

// =========================================================================
// 5. CREDIT ACCOUNT (Tài khoản Tín dụng)
// =========================================================================
class CreditAccount : public Account {
private:
    int64_t m_creditLimitVnd = 0; // Hạn mức tín dụng
    double  m_interestRate = 0.0;  // Lãi suất phạt trả chậm

public:
    CreditAccount(std::string accountId, std::string userId, int64_t usedAmountVnd,
                  int64_t creditLimitVnd, double rate, std::string status, std::string createdAt);

    [[nodiscard]] bool canWithdraw(int64_t amountVnd) const override;
    bool withdraw(int64_t amountVnd) override;
    bool deposit(int64_t amountVnd) override;
    [[nodiscard]] std::string getAccountType() const override;
    [[nodiscard]] int64_t getAvailableCreditVnd() const;
};

} // namespace Banking

#endif // ACCOUNT_MODEL_HPP
