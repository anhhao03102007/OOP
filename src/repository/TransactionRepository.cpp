#include "repository/TransactionRepository.hpp"

namespace Banking {

namespace {
bool transactionExists(TursoDB& db, const std::string& id) {
    const auto response = db.execute("SELECT 1 FROM transactions WHERE transaction_id = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", id}}}));
    try { return !response["results"][0]["response"]["result"]["rows"].empty(); } catch (...) { return false; }
}
}

TransactionRepository::TransactionRepository(TursoDB& tursoDb) : m_tursoDb(tursoDb) {}

std::string TransactionRepository::extractCell(const json& cell) {
    if (cell.is_object() && cell.contains("value") && !cell["value"].is_null()) {
        const auto& value = cell["value"];
        return value.is_string() ? value.get<std::string>() : value.dump();
    }
    return "";
}

// =========================================================================
// THỰC THI CÁC HÀM TRA CỨU SỐ DƯ PHỤC VỤ GIAO DỊCH
// =========================================================================

// 1. Lấy số dư khả dụng thực tế từ CSDL
int64_t TransactionRepository::getAccountBalance(const std::string& accountId) {
    std::string sql = "SELECT balance_vnd FROM accounts WHERE account_id = ? LIMIT 1;";
    json args = json::array({{{"type", "text"}, {"value", accountId}}});

    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                std::string valStr = extractCell(rows[0][0]);
                return valStr.empty() ? 0 : std::stoll(valStr);
            }
        }
    } catch (...) {}
    return 0;
}

// 2. Kiểm tra tài khoản có đủ số dư để chi trả
bool TransactionRepository::hasEnoughBalance(const std::string& accountId, int64_t requiredAmountVnd) {
    if (requiredAmountVnd <= 0) return false;

    std::string sql = "SELECT COUNT(*) FROM accounts WHERE account_id = ? AND balance_vnd >= ? LIMIT 1;";
    json args = json::array({
        {{"type", "text"},    {"value", accountId}},
        {{"type", "numeric"}, {"value", requiredAmountVnd}}
    });

    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                return std::stoi(extractCell(rows[0][0])) > 0;
            }
        }
    } catch (...) {}
    return false;
}

// 3. Tra cứu số dư của tài khoản còn đang ACTIVE
std::optional<int64_t> TransactionRepository::getActiveAccountBalance(const std::string& accountId) {
    std::string sql = "SELECT balance_vnd FROM accounts WHERE account_id = ? AND status = 'ACTIVE' LIMIT 1;";
    json args = json::array({{{"type", "text"}, {"value", accountId}}});

    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                std::string valStr = extractCell(rows[0][0]);
                return std::stoll(valStr.empty() ? "0" : valStr);
            }
        }
    } catch (...) {}
    return std::nullopt;
}

// =========================================================================
// THỰC THI CÁC HÀM LƯU GIAO DỊCH
// =========================================================================

bool TransactionRepository::saveTransfer(const TransferTransaction& txn, const std::string& senderUserId,
                                          const std::string& txnDate, const std::string& idempotencyKey,
                                          int64_t postBalanceVnd, const std::string& responseJson) {
    std::vector<std::pair<std::string, json>> batch;
    const bool reserveIdempotency = !idempotencyKey.empty();
    const std::string normalizedKey = reserveIdempotency ? senderUserId + ":TRANSFER:" + idempotencyKey : std::string{};
    const std::string reservation = "PENDING:" + txn.getTransactionId();
    if (reserveIdempotency) {
        batch.push_back({
            "INSERT INTO idempotency_keys (idempotency_key, user_id, action, response_json, created_at) "
            "VALUES (?, ?, 'TRANSFER', ?, ?) ON CONFLICT(idempotency_key) DO NOTHING;",
            json::array({{{"type", "text"}, {"value", normalizedKey}},
                         {{"type", "text"}, {"value", senderUserId}},
                         {{"type", "text"}, {"value", reservation}},
                         {{"type", "text"}, {"value", txn.getCreatedAt()}}})
        });
    }

    std::string debitSql =
        "UPDATE accounts SET balance_vnd = balance_vnd - ?, last_tx_id = ? WHERE account_id = ? AND status = 'ACTIVE' "
        "AND balance_vnd >= ? AND COALESCE((SELECT total_amount_vnd FROM daily_transaction_limits "
        "WHERE user_id = ? AND txn_date = ?), 0) + ? <= 1000000000";
    if (txn.getReceiverAccountId().rfind("EXT:", 0) != 0) {
        debitSql += " AND EXISTS (SELECT 1 FROM accounts WHERE account_id = ? AND status = 'ACTIVE')";
    }
    if (reserveIdempotency) {
        debitSql += " AND changes() = 1 AND EXISTS (SELECT 1 FROM idempotency_keys "
                    "WHERE idempotency_key = ? AND user_id = ? AND action = 'TRANSFER' AND response_json = ?)";
    }
    debitSql += ";";
    json debitArgs = json::array({
        {{"type", "numeric"}, {"value", txn.getTotalDeductVnd()}},
        {{"type", "text"}, {"value", txn.getTransactionId()}},
        {{"type", "text"},    {"value", txn.getSenderAccountId()}},
        {{"type", "numeric"}, {"value", txn.getTotalDeductVnd()}},
        {{"type", "text"}, {"value", senderUserId}},
        {{"type", "text"}, {"value", txnDate}},
        {{"type", "numeric"}, {"value", txn.getAmountVnd()}}
    });
    if (txn.getReceiverAccountId().rfind("EXT:", 0) != 0) {
        debitArgs.push_back({{"type", "text"}, {"value", txn.getReceiverAccountId()}});
    }
    if (reserveIdempotency) {
        debitArgs.push_back({{"type", "text"}, {"value", normalizedKey}});
        debitArgs.push_back({{"type", "text"}, {"value", senderUserId}});
        debitArgs.push_back({{"type", "text"}, {"value", reservation}});
    }
    batch.push_back({debitSql, debitArgs});

    if (txn.getReceiverAccountId().rfind("EXT:", 0) != 0) batch.push_back({
        "UPDATE accounts SET balance_vnd = balance_vnd + ?, last_tx_id = ? WHERE account_id = ? AND status = 'ACTIVE' AND changes() = 1;",
        json::array({
            {{"type", "numeric"}, {"value", txn.getAmountVnd()}},
            {{"type", "text"}, {"value", txn.getTransactionId()}},
            {{"type", "text"},    {"value", txn.getReceiverAccountId()}}
        })
    });

    batch.push_back({
        "INSERT INTO transactions (transaction_id, sender_account_id, receiver_account_id, amount_vnd, fee_vnd, type, status, description, post_balance_vnd, created_at) "
        "SELECT ?, ?, ?, ?, ?, ?, 'SUCCESS', ?, ?, ? WHERE changes() = 1;",
        json::array({
            {{"type", "text"},    {"value", txn.getTransactionId()}},
            {{"type", "text"},    {"value", txn.getSenderAccountId()}},
            {{"type", "text"},    {"value", txn.getReceiverAccountId()}},
            {{"type", "numeric"}, {"value", txn.getAmountVnd()}},
            {{"type", "numeric"}, {"value", txn.getFeeVnd()}},
            {{"type", "text"},    {"value", txn.getTransactionType()}},
            {{"type", "text"},    {"value", txn.getDescription()}},
            {{"type", "numeric"}, {"value", postBalanceVnd}},
            {{"type", "text"},    {"value", txn.getCreatedAt()}}
        })
    });

    batch.push_back({
        "INSERT INTO daily_transaction_limits (user_id, txn_date, total_amount_vnd) SELECT ?, ?, ? WHERE changes() = 1 "
        "ON CONFLICT(user_id, txn_date) DO UPDATE SET total_amount_vnd = total_amount_vnd + ?;",
        json::array({
            {{"type", "text"},    {"value", senderUserId}},
            {{"type", "text"},    {"value", txnDate}},
            {{"type", "numeric"}, {"value", txn.getAmountVnd()}},
            {{"type", "numeric"}, {"value", txn.getAmountVnd()}}
        })
    });

    if (reserveIdempotency && !responseJson.empty()) {
        batch.push_back({
            "UPDATE idempotency_keys SET response_json = ? WHERE idempotency_key = ? AND user_id = ? AND action = 'TRANSFER' "
            "AND response_json = ? AND EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
            json::array({{{"type", "text"}, {"value", responseJson}},
                         {{"type", "text"}, {"value", normalizedKey}},
                         {{"type", "text"}, {"value", senderUserId}},
                         {{"type", "text"}, {"value", reservation}},
                         {{"type", "text"}, {"value", txn.getTransactionId()}}})
        });
    }
    if (reserveIdempotency) batch.push_back({
        "DELETE FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = 'TRANSFER' "
        "AND response_json = ? AND NOT EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
        json::array({{{"type", "text"}, {"value", normalizedKey}},
                     {{"type", "text"}, {"value", senderUserId}},
                     {{"type", "text"}, {"value", reservation}},
                     {{"type", "text"}, {"value", txn.getTransactionId()}}})
    });

    json res = m_tursoDb.executeBatch(batch);
    if (!res.contains("results") || res["results"].empty() || !transactionExists(m_tursoDb, txn.getTransactionId())) return false;
    if (!reserveIdempotency) return true;
    const auto savedResponse = getIdempotentResponse(idempotencyKey, senderUserId, "TRANSFER");
    return savedResponse && !savedResponse->empty() && savedResponse->rfind("PENDING:", 0) != 0;
}

bool TransactionRepository::saveBillPayment(const BillPaymentTransaction& txn, int64_t postBalanceVnd,
                                             const std::string& userId, const std::string& idempotencyKey,
                                             const std::string& responseJson) {
    std::vector<std::pair<std::string, json>> batch;
    const bool reserveIdempotency = !idempotencyKey.empty() && !userId.empty();
    const std::string normalizedKey = reserveIdempotency ? userId + ":BILL_PAYMENT:" + idempotencyKey : std::string{};
    const std::string reservation = "PENDING:" + txn.getTransactionId();
    if (reserveIdempotency) batch.push_back({
        "INSERT INTO idempotency_keys (idempotency_key, user_id, action, response_json, created_at) "
        "VALUES (?, ?, 'BILL_PAYMENT', ?, ?) ON CONFLICT(idempotency_key) DO NOTHING;",
        json::array({{{"type", "text"}, {"value", normalizedKey}},
                     {{"type", "text"}, {"value", userId}},
                     {{"type", "text"}, {"value", reservation}},
                     {{"type", "text"}, {"value", txn.getCreatedAt()}}})
    });

    std::string debitSql =
        "UPDATE accounts SET balance_vnd = balance_vnd - ? WHERE account_id = ? AND status = 'ACTIVE' "
        "AND balance_vnd >= ? AND EXISTS (SELECT 1 FROM bills WHERE bill_id = ? AND status = 'UNPAID')";
    if (reserveIdempotency) {
        debitSql += " AND changes() = 1 AND EXISTS (SELECT 1 FROM idempotency_keys "
                    "WHERE idempotency_key = ? AND user_id = ? AND action = 'BILL_PAYMENT' AND response_json = ?)";
    }
    debitSql += ";";
    json debitArgs = json::array({
        {{"type", "numeric"}, {"value", txn.getTotalDeductVnd()}},
        {{"type", "text"},    {"value", txn.getPayerAccountId()}},
        {{"type", "numeric"}, {"value", txn.getTotalDeductVnd()}},
        {{"type", "text"}, {"value", txn.getBillId()}}
    });
    if (reserveIdempotency) {
        debitArgs.push_back({{"type", "text"}, {"value", normalizedKey}});
        debitArgs.push_back({{"type", "text"}, {"value", userId}});
        debitArgs.push_back({{"type", "text"}, {"value", reservation}});
    }
    batch.push_back({debitSql, debitArgs});

    batch.push_back({
        "UPDATE bills SET status = 'PAID' WHERE bill_id = ? AND status = 'UNPAID' AND changes() = 1;",
        json::array({{{"type", "text"}, {"value", txn.getBillId()}}})
    });

    batch.push_back({
        "INSERT INTO transactions (transaction_id, sender_account_id, receiver_account_id, amount_vnd, fee_vnd, type, status, description, post_balance_vnd, created_at) "
        "SELECT ?, ?, 'SYSTEM_BILL', ?, ?, ?, 'SUCCESS', ?, ?, ? WHERE changes() = 1;",
        json::array({
            {{"type", "text"},    {"value", txn.getTransactionId()}},
            {{"type", "text"},    {"value", txn.getPayerAccountId()}},
            {{"type", "numeric"}, {"value", txn.getAmountVnd()}},
            {{"type", "numeric"}, {"value", txn.getFeeVnd()}},
            {{"type", "text"},    {"value", txn.getTransactionType()}},
            {{"type", "text"},    {"value", txn.getDescription()}},
            {{"type", "numeric"}, {"value", postBalanceVnd}},
            {{"type", "text"},    {"value", txn.getCreatedAt()}}
        })
    });

    if (reserveIdempotency && !responseJson.empty()) batch.push_back({
        "UPDATE idempotency_keys SET response_json = ? WHERE idempotency_key = ? AND user_id = ? AND action = 'BILL_PAYMENT' "
        "AND response_json = ? AND EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
        json::array({{{"type", "text"}, {"value", responseJson}},
                     {{"type", "text"}, {"value", normalizedKey}},
                     {{"type", "text"}, {"value", userId}},
                     {{"type", "text"}, {"value", reservation}},
                     {{"type", "text"}, {"value", txn.getTransactionId()}}})
    });
    if (reserveIdempotency) batch.push_back({
        "DELETE FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = 'BILL_PAYMENT' "
        "AND response_json = ? AND NOT EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
        json::array({{{"type", "text"}, {"value", normalizedKey}},
                     {{"type", "text"}, {"value", userId}},
                     {{"type", "text"}, {"value", reservation}},
                     {{"type", "text"}, {"value", txn.getTransactionId()}}})
    });

    json res = m_tursoDb.executeBatch(batch);
    if (!res.contains("results") || res["results"].empty() || !transactionExists(m_tursoDb, txn.getTransactionId())) return false;
    if (!reserveIdempotency) return true;
    const auto savedResponse = getIdempotentResponse(idempotencyKey, userId, "BILL_PAYMENT");
    return savedResponse && !savedResponse->empty() && savedResponse->rfind("PENDING:", 0) != 0;
}

std::optional<std::string> TransactionRepository::getIdempotentResponse(const std::string& key, const std::string& userId, const std::string& action) {
    std::string sql = "SELECT response_json FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = ? LIMIT 1;";
    json args = json::array({{{"type", "text"}, {"value", userId + ":" + action + ":" + key}},
                             {{"type", "text"}, {"value", userId}}, {{"type", "text"}, {"value", action}}});
    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                return extractCell(rows[0][0]);
            }
        }
    } catch (...) {}
    return std::nullopt;
}

int64_t TransactionRepository::getDailyTotalVnd(const std::string& userId, const std::string& txnDate) {
    std::string sql = "SELECT total_amount_vnd FROM daily_transaction_limits WHERE user_id = ? AND txn_date = ? LIMIT 1;";
    json args = json::array({
        {{"type", "text"}, {"value", userId}},
        {{"type", "text"}, {"value", txnDate}}
    });
    json res = m_tursoDb.execute(sql, args);
    try {
        if (res.contains("results") && !res["results"].empty()) {
            auto rows = res["results"][0]["response"]["result"]["rows"];
            if (rows.is_array() && !rows.empty()) {
                return std::stoll(extractCell(rows[0][0]));
            }
        }
    } catch (...) {}
    return 0;
}

bool TransactionRepository::executeAtomicTransfer(const TransferTransaction& txn, const std::string& senderUserId,
                                                  const std::string& txnDate, const std::string& idempotencyKey,
                                                  int64_t postBalanceVnd, const std::string& responseJson) {
    return saveTransfer(txn, senderUserId, txnDate, idempotencyKey, postBalanceVnd, responseJson);
}

bool TransactionRepository::saveWithdrawBatch(const WithdrawTransaction& txn, int64_t postBalanceVnd,
                                                const std::string& executionId, const std::string& operatorId,
                                                const std::string& idempotencyKey, const std::string& idempotencyUserId,
                                                const std::string& idempotencyAction, const std::string& responseJson) {
    std::vector<std::pair<std::string, json>> batch;
    const bool reserveIdempotency = !idempotencyKey.empty() && !idempotencyUserId.empty() && !idempotencyAction.empty();
    const std::string normalizedIdempotencyKey = reserveIdempotency
        ? idempotencyUserId + ":" + idempotencyAction + ":" + idempotencyKey : std::string{};
    const std::string reservation = "PENDING:" + txn.getTransactionId();
    if (reserveIdempotency) {
        batch.push_back({
            "INSERT INTO idempotency_keys (idempotency_key, user_id, action, response_json, created_at) "
            "VALUES (?, ?, ?, ?, ?) ON CONFLICT(idempotency_key) DO NOTHING;",
            json::array({{{"type", "text"}, {"value", normalizedIdempotencyKey}},
                         {{"type", "text"}, {"value", idempotencyUserId}},
                         {{"type", "text"}, {"value", idempotencyAction}},
                         {{"type", "text"}, {"value", reservation}},
                         {{"type", "text"}, {"value", txn.getCreatedAt()}}})
        });
    }
    std::string debitSql =
        "UPDATE accounts SET balance_vnd = balance_vnd - ? WHERE account_id = ? AND status = 'ACTIVE' AND balance_vnd >= ?";
    if (reserveIdempotency) debitSql += " AND changes() = 1";
    debitSql += ";";
    batch.push_back({
        debitSql,
        json::array({
            {{"type", "numeric"}, {"value", txn.getTotalDeductVnd()}},
            {{"type", "text"},    {"value", txn.getSourceAccountId()}},
            {{"type", "numeric"}, {"value", txn.getTotalDeductVnd()}}
        })
    });
    batch.push_back({
        "INSERT INTO transactions (transaction_id, sender_account_id, receiver_account_id, amount_vnd, fee_vnd, type, status, description, post_balance_vnd, created_at) SELECT ?, ?, 'ATM_COUNTER', ?, ?, ?, 'SUCCESS', ?, ?, ? WHERE changes() = 1;",
        json::array({
            {{"type", "text"},    {"value", txn.getTransactionId()}},
            {{"type", "text"},    {"value", txn.getSourceAccountId()}},
            {{"type", "numeric"}, {"value", txn.getAmountVnd()}},
            {{"type", "numeric"}, {"value", txn.getFeeVnd()}},
            {{"type", "text"},    {"value", txn.getTransactionType()}},
            {{"type", "text"},    {"value", txn.getDescription()}},
            {{"type", "numeric"}, {"value", postBalanceVnd}},
            {{"type", "text"},    {"value", txn.getCreatedAt()}}
        })
    });
    if (!executionId.empty()) {
        const std::string details = "execution_id=" + executionId + "; amount_vnd=" + std::to_string(txn.getAmountVnd()) +
            "; result=SUCCESS; transaction_id=" + txn.getTransactionId();
        batch.push_back({
            "INSERT INTO audit_logs (log_id, operator_id, action, target_id, details, created_at) "
            "SELECT ?, ?, 'COUNTER_WITHDRAW_SUCCESS', ?, ?, ? WHERE EXISTS "
            "(SELECT 1 FROM transactions WHERE transaction_id = ?);",
            json::array({
                {{"type", "text"}, {"value", executionId}},
                {{"type", "text"}, {"value", operatorId}},
                {{"type", "text"}, {"value", txn.getSourceAccountId()}},
                {{"type", "text"}, {"value", details}},
                {{"type", "text"}, {"value", txn.getCreatedAt()}},
                {{"type", "text"}, {"value", txn.getTransactionId()}}
            })
        });
    }
    if (reserveIdempotency && !responseJson.empty()) {
        batch.push_back({
            "UPDATE idempotency_keys SET response_json = ? WHERE idempotency_key = ? AND user_id = ? AND action = ? "
            "AND response_json = ? AND EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
            json::array({{{"type", "text"}, {"value", responseJson}},
                         {{"type", "text"}, {"value", normalizedIdempotencyKey}},
                         {{"type", "text"}, {"value", idempotencyUserId}},
                         {{"type", "text"}, {"value", idempotencyAction}},
                         {{"type", "text"}, {"value", reservation}},
                         {{"type", "text"}, {"value", txn.getTransactionId()}}})
        });
    }
    if (reserveIdempotency) {
        batch.push_back({
            "DELETE FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = ? "
            "AND response_json = ? AND NOT EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
            json::array({{{"type", "text"}, {"value", normalizedIdempotencyKey}},
                         {{"type", "text"}, {"value", idempotencyUserId}},
                         {{"type", "text"}, {"value", idempotencyAction}},
                         {{"type", "text"}, {"value", reservation}},
                         {{"type", "text"}, {"value", txn.getTransactionId()}}})
        });
    }
    json res = m_tursoDb.executeBatch(batch);
    const bool transactionSaved = res.contains("results") && !res["results"].empty() && transactionExists(m_tursoDb, txn.getTransactionId());
    if (!transactionSaved || executionId.empty()) return transactionSaved;
    const json auditRes = m_tursoDb.execute("SELECT 1 FROM audit_logs WHERE log_id = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", executionId}}}));
    try {
        const auto& rows = auditRes["results"][0]["response"]["result"]["rows"];
        return rows.is_array() && !rows.empty();
    } catch (...) { return false; }
}

bool TransactionRepository::saveDepositBatch(const DepositTransaction& txn, int64_t postBalanceVnd,
                                               const std::string& executionId, const std::string& operatorId,
                                               const std::string& responseJson) {
    std::vector<std::pair<std::string, json>> batch;
    const bool reserveIdempotency = !executionId.empty() && !operatorId.empty();
    const std::string idempotencyAction = "COUNTER_DEPOSIT";
    const std::string normalizedIdempotencyKey = reserveIdempotency
        ? operatorId + ":" + idempotencyAction + ":" + executionId : std::string{};
    const std::string reservation = "PENDING:" + txn.getTransactionId();
    if (reserveIdempotency) {
        batch.push_back({
            "INSERT INTO idempotency_keys (idempotency_key, user_id, action, response_json, created_at) "
            "VALUES (?, ?, ?, ?, ?) ON CONFLICT(idempotency_key) DO NOTHING;",
            json::array({{{"type", "text"}, {"value", normalizedIdempotencyKey}},
                         {{"type", "text"}, {"value", operatorId}},
                         {{"type", "text"}, {"value", idempotencyAction}},
                         {{"type", "text"}, {"value", reservation}},
                         {{"type", "text"}, {"value", txn.getCreatedAt()}}})
        });
    }
    std::string creditSql = "UPDATE accounts SET balance_vnd = balance_vnd + ? WHERE account_id = ? AND status = 'ACTIVE'";
    if (reserveIdempotency) creditSql += " AND changes() = 1";
    creditSql += ";";
    batch.push_back({
        creditSql,
        json::array({
            {{"type", "numeric"}, {"value", txn.getAmountVnd()}},
            {{"type", "text"},    {"value", txn.getTargetAccountId()}}
        })
    });
    batch.push_back({
        "INSERT INTO transactions (transaction_id, sender_account_id, receiver_account_id, amount_vnd, fee_vnd, type, status, description, post_balance_vnd, created_at) SELECT ?, 'COUNTER_CASH', ?, ?, 0, 'DEPOSIT_COUNTER', 'SUCCESS', ?, ?, ? WHERE changes() = 1;",
        json::array({
            {{"type", "text"},    {"value", txn.getTransactionId()}},
            {{"type", "text"},    {"value", txn.getTargetAccountId()}},
            {{"type", "numeric"}, {"value", txn.getAmountVnd()}},
            {{"type", "text"},    {"value", txn.getDescription()}},
            {{"type", "numeric"}, {"value", postBalanceVnd}},
            {{"type", "text"},    {"value", txn.getCreatedAt()}}
        })
    });
    if (!executionId.empty()) {
        const std::string details = "execution_id=" + executionId + "; amount_vnd=" + std::to_string(txn.getAmountVnd()) +
            "; result=SUCCESS; transaction_id=" + txn.getTransactionId();
        batch.push_back({
            "INSERT INTO audit_logs (log_id, operator_id, action, target_id, details, created_at) "
            "SELECT ?, ?, 'DEPOSIT_COUNTER_SUCCESS', ?, ?, ? WHERE EXISTS "
            "(SELECT 1 FROM transactions WHERE transaction_id = ?);",
            json::array({
                {{"type", "text"}, {"value", executionId}},
                {{"type", "text"}, {"value", operatorId}},
                {{"type", "text"}, {"value", txn.getTargetAccountId()}},
                {{"type", "text"}, {"value", details}},
                {{"type", "text"}, {"value", txn.getCreatedAt()}},
                {{"type", "text"}, {"value", txn.getTransactionId()}}
            })
        });
    }
    if (reserveIdempotency && !responseJson.empty()) {
        batch.push_back({
            "UPDATE idempotency_keys SET response_json = ? WHERE idempotency_key = ? AND user_id = ? AND action = ? "
            "AND response_json = ? AND EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
            json::array({{{"type", "text"}, {"value", responseJson}},
                         {{"type", "text"}, {"value", normalizedIdempotencyKey}},
                         {{"type", "text"}, {"value", operatorId}},
                         {{"type", "text"}, {"value", idempotencyAction}},
                         {{"type", "text"}, {"value", reservation}},
                         {{"type", "text"}, {"value", txn.getTransactionId()}}})
        });
    }
    if (reserveIdempotency) {
        batch.push_back({
            "DELETE FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ? AND action = ? "
            "AND response_json = ? AND NOT EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?);",
            json::array({{{"type", "text"}, {"value", normalizedIdempotencyKey}},
                         {{"type", "text"}, {"value", operatorId}},
                         {{"type", "text"}, {"value", idempotencyAction}},
                         {{"type", "text"}, {"value", reservation}},
                         {{"type", "text"}, {"value", txn.getTransactionId()}}})
        });
    }
    json res = m_tursoDb.executeBatch(batch);
    const bool transactionSaved = res.contains("results") && !res["results"].empty() && transactionExists(m_tursoDb, txn.getTransactionId());
    if (!transactionSaved || executionId.empty()) return transactionSaved;
    const json auditRes = m_tursoDb.execute("SELECT 1 FROM audit_logs WHERE log_id = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", executionId}}}));
    try {
        const auto& rows = auditRes["results"][0]["response"]["result"]["rows"];
        return rows.is_array() && !rows.empty();
    } catch (...) { return false; }
}

bool TransactionRepository::saveVirtualDebit(const std::string& accountId, const std::string& userId, int64_t amountVnd,
                                              const std::string& type, const std::string& description, const std::string& transactionId,
                                              const std::string& idempotencyKey, int64_t postBalanceVnd, const std::string& responseJson) {
    std::vector<std::pair<std::string, json>> batch;
    const bool reserveIdempotency = !idempotencyKey.empty();
    const std::string normalizedKey = reserveIdempotency ? userId + ":" + type + ":" + idempotencyKey : std::string{};
    const std::string reservation = "PENDING:" + transactionId;
    if (reserveIdempotency) batch.push_back({
        "INSERT INTO idempotency_keys (idempotency_key,user_id,action,response_json,created_at) "
        "VALUES (?,?,?, ?,CURRENT_TIMESTAMP) ON CONFLICT(idempotency_key) DO NOTHING;",
        json::array({{{"type","text"},{"value",normalizedKey}},{{"type","text"},{"value",userId}},
                     {{"type","text"},{"value",type}},{{"type","text"},{"value",reservation}}})});
    std::string debitSql = "UPDATE accounts SET balance_vnd = balance_vnd - ? WHERE account_id = ? AND status = 'ACTIVE' AND balance_vnd >= ?";
    if (reserveIdempotency) debitSql += " AND changes() = 1 AND EXISTS (SELECT 1 FROM idempotency_keys WHERE idempotency_key=? AND user_id=? AND action=? AND response_json=?)";
    debitSql += ";";
    json debitArgs = json::array({{{"type","numeric"},{"value",amountVnd}},{{"type","text"},{"value",accountId}},{{"type","numeric"},{"value",amountVnd}}});
    if (reserveIdempotency) {
        debitArgs.push_back({{"type","text"},{"value",normalizedKey}});
        debitArgs.push_back({{"type","text"},{"value",userId}});
        debitArgs.push_back({{"type","text"},{"value",type}});
        debitArgs.push_back({{"type","text"},{"value",reservation}});
    }
    batch.push_back({debitSql, debitArgs});
    batch.push_back({"INSERT INTO transactions (transaction_id,sender_account_id,receiver_account_id,amount_vnd,fee_vnd,type,status,description,post_balance_vnd,created_at) SELECT ?,?,'SIMULATED_PROVIDER',?,0,?,'SUCCESS',?,?,CURRENT_TIMESTAMP WHERE changes()=1;",
        json::array({{{"type","text"},{"value",transactionId}},{{"type","text"},{"value",accountId}},{{"type","numeric"},{"value",amountVnd}},{{"type","text"},{"value",type}},{{"type","text"},{"value",description}},{{"type","numeric"},{"value",postBalanceVnd}}})});
    if (reserveIdempotency && !responseJson.empty()) batch.push_back({
        "UPDATE idempotency_keys SET response_json=? WHERE idempotency_key=? AND user_id=? AND action=? AND response_json=? AND EXISTS(SELECT 1 FROM transactions WHERE transaction_id=?);",
        json::array({{{"type","text"},{"value",responseJson}},{{"type","text"},{"value",normalizedKey}},
                     {{"type","text"},{"value",userId}},{{"type","text"},{"value",type}},
                     {{"type","text"},{"value",reservation}},{{"type","text"},{"value",transactionId}}})});
    if (reserveIdempotency) batch.push_back({
        "DELETE FROM idempotency_keys WHERE idempotency_key=? AND user_id=? AND action=? AND response_json=? AND NOT EXISTS(SELECT 1 FROM transactions WHERE transaction_id=?);",
        json::array({{{"type","text"},{"value",normalizedKey}},{{"type","text"},{"value",userId}},
                     {{"type","text"},{"value",type}},{{"type","text"},{"value",reservation}},
                     {{"type","text"},{"value",transactionId}}})});
    const auto response = m_tursoDb.executeBatch(batch);
    if (!response.contains("results") || response["results"].empty() || !transactionExists(m_tursoDb, transactionId)) return false;
    if (!reserveIdempotency) return true;
    const auto savedResponse = getIdempotentResponse(idempotencyKey, userId, type);
    return savedResponse && !savedResponse->empty() && savedResponse->rfind("PENDING:", 0) != 0;
}

bool TransactionRepository::saveBillPaymentBatch(const BillPaymentTransaction& txn, int64_t postBalanceVnd,
                                                   const std::string& userId, const std::string& idempotencyKey,
                                                   const std::string& responseJson) {
    return saveBillPayment(txn, postBalanceVnd, userId, idempotencyKey, responseJson);
}

} // namespace Banking
