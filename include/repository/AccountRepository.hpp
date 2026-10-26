#ifndef ACCOUNT_REPOSITORY_HPP
#define ACCOUNT_REPOSITORY_HPP

#include <string>
#include <vector>
#include <optional>
#include <core/json.hpp>
#include "model/AccountModel.hpp"
#include "core/TursoDB.hpp"
#include "dtos/AccountDTOs.hpp"
#include "dtos/TransactionDTOs.hpp"

namespace Banking {

using json = nlohmann::json;

class AccountRepository {
private:
    TursoDB& m_tursoDb;

    static std::string extractCell(const json& cell);

public:
    explicit AccountRepository(TursoDB& tursoDb);

    // 1. Lấy tất cả tài khoản thuộc sở hữu của 1 User ID (Dạng DTO)
    std::vector<AccountDTO> getAccountsByUserId(const std::string& userId);

    // 2. Nạp CheckingAccount Model lên RAM
    std::optional<CheckingAccount> findCheckingAccountById(const std::string& accountId);

    // 3. Thực thi Atomic Batch Mở tài khoản tiết kiệm mới & Trừ tiền tài khoản thanh toán
    bool executeOpenSavingsBatch(const std::string& senderAccId, int64_t senderPostBalance,
                                 const std::string& newSavingsAccId, const std::string& userId,
                                 int64_t depositAmount, double interestRate, const std::string& createdAt);

    // 4. Cập nhật trạng thái tài khoản (ACTIVE / BLOCKED)
    bool updateAccountStatus(const std::string& accountId, const std::string& newStatus);

    // 5. Kiểm tra quyền sở hữu tài khoản
    bool isAccountOwnedByUser(const std::string& accountId, const std::string& userId);

    // 6. Tạo tài khoản thanh toán mặc định
    bool createCheckingAccount(const std::string& accountId, const std::string& userId);

    // 7. Tạo tài khoản mới tổng quát
    bool createNewAccount(const std::string& accountId, const std::string& userId,
                         const std::string& accountType, int64_t initialBalanceVnd,
                         const std::string& status, const std::string& createdAt);

    // 8. Lấy số dư tài khoản
    int64_t getBalance(const std::string& accountId);

    // 9. Rút/Trừ tiền tài khoản
    bool withdraw(const std::string& accountId, int64_t amountVnd);

    // 10. Truy vấn lịch sử giao dịch phân trang
    TransactionHistoryResult getHistoryPaginated(const std::string& accountId, int page, int limit);
};

} // namespace Banking

#endif // ACCOUNT_REPOSITORY_HPP
