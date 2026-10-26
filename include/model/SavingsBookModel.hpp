#ifndef SAVINGS_BOOK_MODEL_HPP
#define SAVINGS_BOOK_MODEL_HPP

#include <string>
#include <vector>
#include <optional>
#include <utility>
#include <cstdint>
#include "model/AccountModel.hpp"

namespace Banking {

class SavingsBook {
private:
    std::string m_userId;
    std::vector<TermSavingsAccount> m_ds; // Danh sách các sổ tiết kiệm có kỳ hạn

public:
    SavingsBook() = default;
    explicit SavingsBook(std::string userId);
    SavingsBook(std::string userId, std::vector<TermSavingsAccount> ds);

    // --- HÀM QUẢN LÝ NGHIỆP VỤ SỔ TIẾT KIỆM (PURE OOP) ---
    void addAccount(const TermSavingsAccount& account);

    [[nodiscard]] std::optional<TermSavingsAccount> findById(const std::string& savingsId) const;
    [[nodiscard]] std::vector<TermSavingsAccount> getActiveAccounts() const;
    [[nodiscard]] int64_t getTotalPrincipalVnd() const;
    [[nodiscard]] int64_t getTotalExpectedInterestVnd() const;

    // Thực thi đóng/tất toán sổ tiết kiệm
    std::pair<bool, int64_t> closeAccount(const std::string& savingsId, int actualDaysHeld = -1);

    // --- READ-ONLY GETTERS ---
    [[nodiscard]] const std::string& getUserId() const;
    [[nodiscard]] const std::vector<TermSavingsAccount>& getList() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;
};

} // namespace Banking

#endif // SAVINGS_BOOK_MODEL_HPP