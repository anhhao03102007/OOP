#include "model/TransactionModel.hpp"
#include <algorithm>
#include <ctime>
#include <chrono>
#include <cstdlib>
#include <utility>
#include <core/AuthSecurity.hpp>

namespace Banking {

// =========================================================================
// 1. BASE CLASS: Transaction
// =========================================================================
Transaction::Transaction(int64_t amountVnd, std::string desc)
    : m_amountVnd(amountVnd), m_description(std::move(desc)) {
    
    m_transactionId = "TXN_" + AuthSecurity::generateNumericCode(24);
    
    char buf[32];
    std::time_t now = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    m_createdAt = std::string(buf);
}

int64_t Transaction::getTotalDeductVnd() const { return m_amountVnd + m_feeVnd; }
const std::string& Transaction::getTransactionId() const { return m_transactionId; }
int64_t Transaction::getAmountVnd() const { return m_amountVnd; }
int64_t Transaction::getFeeVnd() const { return m_feeVnd; }
const std::string& Transaction::getStatus() const { return m_status; }
const std::string& Transaction::getDescription() const { return m_description; }
const std::string& Transaction::getCreatedAt() const { return m_createdAt; }

// =========================================================================
// 2. TRANSFER TRANSACTION
// =========================================================================
TransferTransaction::TransferTransaction(CheckingAccount& sender, CheckingAccount& receiver, int64_t amountVnd,
                                         std::string desc, bool isInterbank, int64_t interbankFeeVnd)
    : Transaction(amountVnd, std::move(desc)),
      m_sender(sender), m_receiver(receiver), m_isInterbank(isInterbank),
      m_interbankFeeVnd(std::max<int64_t>(0, interbankFeeVnd)) {
    m_feeVnd = calculateFee();
}

bool TransferTransaction::process() {
    int64_t totalDeduct = getTotalDeductVnd();

    if (!m_sender.canWithdraw(totalDeduct)) {
        m_status = "FAILED";
        return false;
    }

    if (m_sender.withdraw(totalDeduct)) {
        if (m_receiver.deposit(m_amountVnd)) {
            m_status = "SUCCESS";
            return true;
        } else {
            m_sender.deposit(totalDeduct); // Rollback in-memory
        }
    }

    m_status = "FAILED";
    return false;
}

int64_t TransferTransaction::calculateFee() const {
    return m_isInterbank ? m_interbankFeeVnd : 0;
}

std::string TransferTransaction::getTransactionType() const {
    return m_isInterbank ? "TRANSFER_INTERBANK" : "TRANSFER_INTERNAL";
}

const std::string& TransferTransaction::getSenderAccountId() const { return m_sender.getAccountId(); }
const std::string& TransferTransaction::getReceiverAccountId() const { return m_receiver.getAccountId(); }

// =========================================================================
// 3. DEPOSIT TRANSACTION
// =========================================================================
DepositTransaction::DepositTransaction(CheckingAccount& targetAccount, int64_t amountVnd, std::string desc)
    : Transaction(amountVnd, std::move(desc)), m_targetAccount(targetAccount) {
    m_feeVnd = calculateFee();
}

bool DepositTransaction::process() {
    if (m_targetAccount.deposit(m_amountVnd)) {
        m_status = "SUCCESS";
        return true;
    }
    m_status = "FAILED";
    return false;
}

int64_t DepositTransaction::calculateFee() const { return 0; }
std::string DepositTransaction::getTransactionType() const { return "DEPOSIT_COUNTER"; }
const std::string& DepositTransaction::getTargetAccountId() const { return m_targetAccount.getAccountId(); }

// =========================================================================
// 4. WITHDRAW TRANSACTION
// =========================================================================
WithdrawTransaction::WithdrawTransaction(CheckingAccount& sourceAccount, int64_t amountVnd,
                                         std::string desc, bool isAtmOffNetwork, int64_t atmOffNetworkFeeVnd)
    : Transaction(amountVnd, std::move(desc)),
      m_sourceAccount(sourceAccount), m_isAtmOffNetwork(isAtmOffNetwork),
      m_atmOffNetworkFeeVnd(std::max<int64_t>(0, atmOffNetworkFeeVnd)) {
    m_feeVnd = calculateFee();
}

bool WithdrawTransaction::process() {
    if (m_sourceAccount.withdraw(getTotalDeductVnd())) {
        m_status = "SUCCESS";
        return true;
    }
    m_status = "FAILED";
    return false;
}

int64_t WithdrawTransaction::calculateFee() const {
    return m_isAtmOffNetwork ? m_atmOffNetworkFeeVnd : 0;
}

std::string WithdrawTransaction::getTransactionType() const {
    return m_isAtmOffNetwork ? "WITHDRAW_ATM_OFFNET" : "WITHDRAW_COUNTER";
}

const std::string& WithdrawTransaction::getSourceAccountId() const { return m_sourceAccount.getAccountId(); }

// =========================================================================
// 5. BILL PAYMENT TRANSACTION
// =========================================================================
BillPaymentTransaction::BillPaymentTransaction(CheckingAccount& payerAccount, BillModel& bill, std::string desc)
    : Transaction(bill.getAmountVnd(), std::move(desc)),
      m_payerAccount(payerAccount), m_bill(bill) {
    m_feeVnd = calculateFee();
}

bool BillPaymentTransaction::process() {
    if (!m_bill.isPayable()) {
        m_status = "FAILED";
        return false;
    }

    if (m_payerAccount.withdraw(getTotalDeductVnd())) {
        m_bill.markAsPaid();
        m_status = "SUCCESS";
        return true;
    }

    m_status = "FAILED";
    return false;
}

int64_t BillPaymentTransaction::calculateFee() const { return 0; }
std::string BillPaymentTransaction::getTransactionType() const {
    return "PAY_BILL_" + m_bill.getServiceType();
}

const std::string& BillPaymentTransaction::getPayerAccountId() const { return m_payerAccount.getAccountId(); }
const std::string& BillPaymentTransaction::getBillId() const { return m_bill.getBillId(); }

} // namespace Banking
