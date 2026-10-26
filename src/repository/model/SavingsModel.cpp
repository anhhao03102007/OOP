#include "model/SavingsBookModel.hpp"

namespace Banking {

SavingsBook::SavingsBook(std::string userId) 
    : m_userId(std::move(userId)) {}

SavingsBook::SavingsBook(std::string userId, std::vector<TermSavingsAccount> ds)
    : m_userId(std::move(userId)), m_ds(std::move(ds)) {}

void SavingsBook::addAccount(const TermSavingsAccount& account) {
    m_ds.push_back(account);
}

std::optional<TermSavingsAccount> SavingsBook::findById(const std::string& savingsId) const {
    for (const auto& acc : m_ds) {
        if (acc.getAccountId() == savingsId) {
            return acc;
        }
    }
    return std::nullopt;
}

std::vector<TermSavingsAccount> SavingsBook::getActiveAccounts() const {
    std::vector<TermSavingsAccount> activeList;
    for (const auto& acc : m_ds) {
        if (acc.isActive()) {
            activeList.push_back(acc);
        }
    }
    return activeList;
}

int64_t SavingsBook::getTotalPrincipalVnd() const {
    int64_t total = 0;
    for (const auto& acc : m_ds) {
        if (acc.isActive()) {
            total += acc.getBalanceVnd();
        }
    }
    return total;
}

int64_t SavingsBook::getTotalExpectedInterestVnd() const {
    int64_t totalInterest = 0;
    for (const auto& acc : m_ds) {
        if (acc.isActive()) {
            totalInterest += acc.calculateMaturityInterest();
        }
    }
    return totalInterest;
}

std::pair<bool, int64_t> SavingsBook::closeAccount(const std::string& savingsId, int actualDaysHeld) {
    for (auto& acc : m_ds) {
        if (acc.getAccountId() == savingsId && acc.isActive()) {
            int64_t interestVnd = 0;
            
            if (actualDaysHeld >= 0) {
                // Rút trước hạn -> Tính lãi không kỳ hạn (0.1%/năm)
                interestVnd = acc.calculateEarlyWithdrawalInterest(actualDaysHeld);
            } else {
                // Tất toán đúng hạn -> Tính lãi có kỳ hạn
                interestVnd = acc.calculateMaturityInterest();
            }

            int64_t totalPayout = acc.getBalanceVnd() + interestVnd;

            if (acc.withdraw(acc.getBalanceVnd())) { // Chuyển trạng thái sang CLOSED
                return {true, totalPayout};
            }
        }
    }
    return {false, 0};
}

const std::string& SavingsBook::getUserId() const { return m_userId; }
const std::vector<TermSavingsAccount>& SavingsBook::getList() const { return m_ds; }
size_t SavingsBook::size() const { return m_ds.size(); }
bool SavingsBook::empty() const { return m_ds.empty(); }

} // namespace Banking