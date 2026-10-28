#ifndef TRANSACTION_REPOSITORY_HPP
#define TRANSACTION_REPOSITORY_HPP

#include <string>
#include <vector>
#include <optional>
#include <utility>
#include <core/json.hpp>
#include "core/TursoDB.hpp"
#include "model/TransactionModel.hpp"

namespace Banking {

using json = nlohmann::json;

class TransactionRepository {
private:
    TursoDB& m_tursoDb;

    static std::string extractCell(const json& cell);

public:
    explicit TransactionRepository(TursoDB& tursoDb);

    // =========================================================================
    // CÁC HÀM TRA CỨU SỐ DƯ PHỤC VỤ GIAO DỊCH (MỚI BỔ SUNG)
    // =========================================================================
    // 1. Lấy số dư khả dụng thực tế từ CSDL theo Account ID
    int64_t getAccountBalance(const std::string& accountId);

    // 2. Kiểm tra tài khoản có đủ số dư để thực hiện giao dịch hay không
    bool hasEnoughBalance(const std::string& accountId, int64_t requiredAmountVnd);

    // 3. Tra cứu số dư đồng thời kiểm tra trạng thái tài khoản có ACTIVE không
    std::optional<int64_t> getActiveAccountBalance(const std::string& accountId);

    // =========================================================================
    // CÁC HÀM LƯU GIAO DỊCH & BATCH ATOMIC
    // =========================================================================
    // Lưu giao dịch Chuyển tiền (Transfer)
    bool saveTransfer(const TransferTransaction& txn, const std::string& senderUserId,
                      const std::string& txnDate, const std::string& idempotencyKey,
                      int64_t postBalanceVnd, const std::string& responseJson);

    // Lưu giao dịch Thanh toán Hóa đơn (Bill Payment)
    bool saveBillPayment(const BillPaymentTransaction& txn, int64_t postBalanceVnd,
                         const std::string& userId = "", const std::string& idempotencyKey = "",
                         const std::string& responseJson = "");

    // Tra cứu Idempotency Key
    std::optional<std::string> getIdempotentResponse(const std::string& key, const std::string& userId,
                                                      const std::string& action = "TRANSFER");

    // Lấy tổng tiền giao dịch trong ngày
    int64_t getDailyTotalVnd(const std::string& userId, const std::string& txnDate);

    // Alias tên hàm Chuyển tiền Atomic
    bool executeAtomicTransfer(const TransferTransaction& txn, const std::string& senderUserId,
                              const std::string& txnDate, const std::string& idempotencyKey,
                              int64_t postBalanceVnd, const std::string& responseJson);

    // Lưu giao dịch Rút tiền
    bool saveWithdrawBatch(const WithdrawTransaction& txn, int64_t postBalanceVnd,
                           const std::string& executionId = "", const std::string& operatorId = "",
                           const std::string& idempotencyKey = "", const std::string& idempotencyUserId = "",
                           const std::string& idempotencyAction = "", const std::string& responseJson = "");

    // Lưu giao dịch Nạp tiền mặt tại quầy
    bool saveDepositBatch(const DepositTransaction& txn, int64_t postBalanceVnd,
                          const std::string& executionId = "", const std::string& operatorId = "",
                          const std::string& responseJson = "");
    bool saveVirtualDebit(const std::string& accountId, const std::string& userId, int64_t amountVnd,
                          const std::string& type, const std::string& description, const std::string& transactionId,
                          const std::string& idempotencyKey, int64_t postBalanceVnd, const std::string& responseJson);

    // Alias tên hàm Thanh toán hóa đơn
    bool saveBillPaymentBatch(const BillPaymentTransaction& txn, int64_t postBalanceVnd,
                              const std::string& userId = "", const std::string& idempotencyKey = "",
                              const std::string& responseJson = "");
};

} // namespace Banking

#endif // TRANSACTION_REPOSITORY_HPP
