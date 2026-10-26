#ifndef PAGE_DATA_LOADER_SERVICE_HPP
#define PAGE_DATA_LOADER_SERVICE_HPP

#include <string>
#include <core/json.hpp>
#include "dtos/UserDTOs.hpp"
#include "services/UserService.hpp"
#include "services/AccountService.hpp"
#include "services/LoansService.hpp"
#include "services/SavingsService.hpp"
#include "services/NotificationService.hpp"

namespace Banking {

using json = nlohmann::json;

// Gom dữ liệu từ các service thành payload đúng với các sink của giao diện khách hàng.
class PageDataLoaderService {
private:
    UserService& m_userService;
    AccountService& m_accountService;
    LoanService& m_loanService;
    SavingsService& m_savingsService;
    NotificationService& m_notificationService;

public:
    PageDataLoaderService(UserService& userSvc, AccountService& accountSvc, LoanService& loanSvc,
                          SavingsService& savingsSvc, NotificationService& notificationSvc);

    // page là route hiện tại của UI, ví dụ "home", "accounts", "transactions".
    json loadDataForPage(const std::string& page, const std::string& sessionToken);

private:
    json loadHomePageDataForCUS(const UserProfileDTO& profile, const std::string& sessionToken);
    json loadAccountsPageData(const UserProfileDTO& profile);
    json loadServicePageData(const UserProfileDTO& profile);
    json loadSettingPageData(const UserProfileDTO& profile);
    json loadTransactionsPageData(const UserProfileDTO& profile, const std::string& sessionToken);
    json loadLoanPageData(const UserProfileDTO& profile, const std::string& sessionToken);
    json loadSavingsPageData(const UserProfileDTO& profile, const std::string& sessionToken);
};

} // namespace Banking

#endif // PAGE_DATA_LOADER_SERVICE_HPP
