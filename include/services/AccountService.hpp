#ifndef ACCOUNT_SERVICE_HPP
#define ACCOUNT_SERVICE_HPP

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <ctime>

#include "dtos/AccountDTOs.hpp"
#include "dtos/TransactionDTOs.hpp"
#include "model/UserModel.hpp"
#include "repository/AccountRepository.hpp"
#include "repository/UserRepository.hpp"
#include "repository/AuditRepository.hpp"

namespace Banking {

class AccountService {
private:
    AccountRepository& m_accountRepo;
    UserRepository&    m_userRepo;
    AuditRepository&   m_auditRepo;

    // Helper: Sinh Số tài khoản / Mã hợp đồng mới (Tiền tố 10 + Timestamp miliseconds)
    static std::string generateNewAccountId(const std::string& type);

public:
    AccountService(AccountRepository& accountRepo, UserRepository& userRepo, AuditRepository& auditRepo);

    // =========================================================================
    // TÍNH NĂNG 1: XEM DANH SÁCH & SỐ DƯ TÀI KHOẢN (RBAC ENFORCEMENT)
    // =========================================================================
    GetAccountsResult getAccounts(const std::string& operatorUserId, const std::string& targetUserId);

    // =========================================================================
    // TÍNH NĂNG 2: TRA CỨU LỊCH SỬ GIAO DỊCH / IN SAO KÊ TÀI KHOẢN
    // =========================================================================
    TransactionHistoryResult getTransactionHistory(const std::string& operatorUserId, 
                                                   const std::string& stk, 
                                                   int limit = 50, 
                                                   int page = 1);

    // =========================================================================
    // TÍNH NĂNG 3: MỞ TÀI KHOẢN MỚI (eKYC KHÁCH HÀNG / GDV MỞ TẠI QUẦY)
    // =========================================================================
    TransactionResult openNewAccount(const std::string& operatorUserId, 
                                     const std::string& targetUserId, 
                                     const std::string& accountType,
                                     const std::string& customerPin,
                                     bool kycVerified,
                                     int64_t initialBalanceVnd = 0);

    // =========================================================================
    // TÍNH NĂNG 4: KHÓA / MỞ KHÓA TÀI KHOẢN KHẨN CẤP
    // =========================================================================
    TransactionResult toggleAccountLock(const std::string& operatorUserId, 
                                        const std::string& stk, 
                                        bool lockRequest, 
                                        const std::string& pinOrVerifyCode);
};

} // namespace Banking

#endif // ACCOUNT_SERVICE_HPP
