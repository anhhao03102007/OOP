#ifndef TRANSACTION_MODEL_HPP
#define TRANSACTION_MODEL_HPP

#include <string>
#include <cstdint>
#include "model/AccountModel.hpp"
#include "model/BillModel.hpp"

namespace Banking {

// =========================================================================
// 1. BASE CLASS: Transaction (Command Interface)
// =========================================================================
class Transaction {
protected:
    std::string m_transactionId;
    int64_t     m_amountVnd = 0;
    int64_t     m_feeVnd = 0;
    std::string m_description;
    std::string m_status = "PENDING"; // "PENDING", "SUCCESS", "FAILED"
    std::string m_createdAt;

public:
    Transaction(int64_t amountVnd, std::string desc);
    virtual ~Transaction() = default;

    // --- LOGIC THỰC THI NGHIỆP VỤ ĐA HÌNH ---
    virtual bool process() = 0;
    [[nodiscard]] virtual int64_t calculateFee() const = 0;
    [[nodiscard]] virtual std::string getTransactionType() const = 0;

    [[nodiscard]] int64_t getTotalDeductVnd() const;
    [[nodiscard]] const std::string& getTransactionId() const;
    [[nodiscard]] int64_t getAmountVnd() const;
    [[nodiscard]] int64_t getFeeVnd() const;
    [[nodiscard]] const std::string& getStatus() const;
    [[nodiscard]] const std::string& getDescription() const;
    [[nodiscard]] const std::string& getCreatedAt() const;
};

// =========================================================================
// 2. TRANSFER TRANSACTION (Chuyển tiền Nội bộ / Liên ngân hàng)
// =========================================================================
class TransferTransaction : public Transaction {
private:
    CheckingAccount& m_sender;
    CheckingAccount& m_receiver;
    bool             m_isInterbank = false;
    int64_t          m_interbankFeeVnd = 9900;

public:
    TransferTransaction(CheckingAccount& sender, CheckingAccount& receiver, int64_t amountVnd,
                        std::string desc, bool isInterbank = false, int64_t interbankFeeVnd = 9900);

    bool process() override;
    [[nodiscard]] int64_t calculateFee() const override;
    [[nodiscard]] std::string getTransactionType() const override;

    [[nodiscard]] const std::string& getSenderAccountId() const;
    [[nodiscard]] const std::string& getReceiverAccountId() const;
};

// =========================================================================
// 3. DEPOSIT TRANSACTION (Nạp tiền mặt tại quầy)
// =========================================================================
class DepositTransaction : public Transaction {
private:
    CheckingAccount& m_targetAccount;

public:
    DepositTransaction(CheckingAccount& targetAccount, int64_t amountVnd, std::string desc);

    bool process() override;
    [[nodiscard]] int64_t calculateFee() const override;
    [[nodiscard]] std::string getTransactionType() const override;
    [[nodiscard]] const std::string& getTargetAccountId() const;
};

// =========================================================================
// 4. WITHDRAW TRANSACTION (Rút tiền mặt tại quầy / ATM)
// =========================================================================
class WithdrawTransaction : public Transaction {
private:
    CheckingAccount& m_sourceAccount;
    bool             m_isAtmOffNetwork = false;
    int64_t          m_atmOffNetworkFeeVnd = 3300;

public:
    WithdrawTransaction(CheckingAccount& sourceAccount, int64_t amountVnd,
                        std::string desc, bool isAtmOffNetwork = false, int64_t atmOffNetworkFeeVnd = 3300);

    bool process() override;
    [[nodiscard]] int64_t calculateFee() const override;
    [[nodiscard]] std::string getTransactionType() const override;
    [[nodiscard]] const std::string& getSourceAccountId() const;
};

// =========================================================================
// 5. BILL PAYMENT TRANSACTION (Thanh toán hóa đơn cước)
// =========================================================================
class BillPaymentTransaction : public Transaction {
private:
    CheckingAccount& m_payerAccount;
    BillModel&       m_bill;

public:
    BillPaymentTransaction(CheckingAccount& payerAccount, BillModel& bill, std::string desc);

    bool process() override;
    [[nodiscard]] int64_t calculateFee() const override;
    [[nodiscard]] std::string getTransactionType() const override;

    [[nodiscard]] const std::string& getPayerAccountId() const;
    [[nodiscard]] const std::string& getBillId() const;
};

} // namespace Banking

#endif // TRANSACTION_MODEL_HPP
