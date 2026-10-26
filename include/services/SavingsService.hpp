#ifndef SAVINGS_SERVICE_HPP
#define SAVINGS_SERVICE_HPP

#include <string>
#include <ctime>
#include <vector>
#include <utility>

#include "dtos/SavingsDTOs.hpp"
#include "repository/SavingsRepository.hpp"
#include "repository/AccountRepository.hpp"
#include "repository/UserRepository.hpp"
#include "repository/AuditRepository.hpp"
#include "core/SessionManager.hpp"
#include "core/TursoDB.hpp"

namespace Banking {

class SavingsService {
private:
    SavingsRepository& m_savingsRepo;
    AccountRepository& m_accRepo;
    UserRepository&    m_userRepo;
    AuditRepository&   m_auditRepo;
    SessionManager&    m_sessionManager;
    TursoDB&           m_tursoDb;

public:
    SavingsService(SavingsRepository& savingsRepo, AccountRepository& accRepo,
                   UserRepository& userRepo, AuditRepository& auditRepo,
                   SessionManager& sessionManager, TursoDB& tursoDb);

    // 1. MỞ SỔ TIẾT KIỆM (ATOMIC TRANSACTION)
    SavingsOperationResult openSavings(const OpenSavingsRequest& req);

    // 2. TẤT TOÁN SỔ TIẾT KIỆM (ĐÚNG HẠN Hoặc TRƯỚC HẠN)
    SavingsOperationResult closeSavings(const CloseSavingsRequest& req);

    // 3. XEM DANH SÁCH SỔ TIẾT KIỆM & TÍNH TOÁN TỔNG DỰ TÍNH (SỬ DỤNG GetSavingsListRequest/Result)
    GetSavingsListResult getSavingsList(const GetSavingsListRequest& req);
};

} // namespace Banking

#endif // SAVINGS_SERVICE_HPP