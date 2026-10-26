        #include "services/PageDataLoaderService.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

namespace Banking {
namespace {

json failure(const std::string& page, const std::string& message) {
    return {
        {"success", false},
        {"page", page},
        {"message", message},
        {"data", json::object()}
    };
}

std::string loanType(const LoanModel& loan) {
    return loan.getCollateralInfo().empty() ? "Vay tiêu dùng" : "Vay thế chấp";
}

int loanStage(const std::string& status) {
    if (status == "APPROVED") return 3;
    if (status == "DISBURSED" || status == "CLOSED") return 4;
    if (status == "PAID") return 5;
    if (status == "REJECTED") return -1;
    return 1; // PENDING / đang thẩm định
}

json missingLoanDocuments() {
    return json::array({
        {{"id", "cccd"}, {"icon", "shield"}, {"name", "CCCD gắn chip (2 mặt)"},
         {"status", "MISSING"}, {"hint", "Chưa có tệp"}, {"file", ""}, {"mime", ""}, {"size", ""}, {"at", ""}, {"url", ""}},
        {{"id", "income"}, {"icon", "wallet"}, {"name", "Sao kê lương / HĐLĐ 3 tháng gần nhất"},
         {"status", "MISSING"}, {"hint", "Chưa có tệp"}, {"file", ""}, {"mime", ""}, {"size", ""}, {"at", ""}, {"url", ""}},
        {{"id", "collateral"}, {"icon", "file"}, {"name", "Giấy tờ tài sản đảm bảo"},
         {"status", "MISSING"}, {"hint", "Chưa có tệp"}, {"file", ""}, {"mime", ""}, {"size", ""}, {"at", ""}, {"url", ""}},
        {{"id", "address"}, {"icon", "home"}, {"name", "Xác nhận nơi cư trú"},
         {"status", "MISSING"}, {"hint", "Chưa có tệp"}, {"file", ""}, {"mime", ""}, {"size", ""}, {"at", ""}, {"url", ""}}
    });
}

} // namespace

PageDataLoaderService::PageDataLoaderService(UserService& userSvc, AccountService& accountSvc,
                                             LoanService& loanSvc, SavingsService& savingsSvc,
                                             NotificationService& notificationSvc)
    : m_userService(userSvc), m_accountService(accountSvc), m_loanService(loanSvc),
      m_savingsService(savingsSvc), m_notificationService(notificationSvc) {}

json PageDataLoaderService::loadDataForPage(const std::string& page, const std::string& sessionToken) {
    try {
        const UserProfileDTO profile = m_userService.getProfile(sessionToken);
        if (profile.user_id.empty()) {
            return failure(page, "Phiên đăng nhập đã hết hạn hoặc không hợp lệ.");
        }
        if (profile.role != "CUSTOMER") {
            return failure(page, "Nạp dữ liệu cho giao diện nhân viên chưa được triển khai.");
        }

        json data;
        if (page == "home") {
            data = loadHomePageDataForCUS(profile, sessionToken);
        } else if (page == "accounts") {
            data = loadAccountsPageData(profile);
        } else if (page == "transactions") {
            data = loadTransactionsPageData(profile, sessionToken);
        } else if (page == "user-settings") {
            data = loadSettingPageData(profile);
        } else if (page == "notifications") {
            data = loadSettingPageData(profile);
        } else if (page == "savings") {
            data = loadSavingsPageData(profile, sessionToken);
        } else if (page == "services" || page == "transfer" || page == "topup" || page == "payment") {
            data = loadServicePageData(profile);
        } else if (page == "apply-loan" || page == "loan" || page == "loan-pay") {
            data = loadLoanPageData(profile, sessionToken);
        } else {
            return failure(page, "Trang này chưa có nguồn dữ liệu backend.");
        }

        const auto notifications = m_notificationService.list(sessionToken);
        if (!notifications.success) return failure(page, notifications.message);
        data["messages"] = notifications.notifications;
        return {
            {"success", true},
            {"page", page},
            {"message", "Nạp dữ liệu thành công."},
            {"data", data}
        };
    } catch (const std::exception& error) {
        return failure(page, error.what());
    } catch (...) {
        return failure(page, "Không thể nạp dữ liệu từ cơ sở dữ liệu.");
    }
}

json PageDataLoaderService::loadHomePageDataForCUS(const UserProfileDTO& profile, const std::string& sessionToken) {
    return loadTransactionsPageData(profile, sessionToken);
}

json PageDataLoaderService::loadAccountsPageData(const UserProfileDTO& profile) {
    const GetAccountsResult result = m_accountService.getAccounts(profile.user_id, profile.user_id);
    if (!result.success) {
        throw std::runtime_error(result.message.empty() ? "Không thể tải danh sách tài khoản." : result.message);
    }
    return {
        {"profile", profile},
        {"accounts", result.accounts}
    };
}

json PageDataLoaderService::loadServicePageData(const UserProfileDTO& profile) {
    return loadAccountsPageData(profile);
}

json PageDataLoaderService::loadSettingPageData(const UserProfileDTO& profile) {
    return {{"profile", profile}};
}

json PageDataLoaderService::loadSavingsPageData(const UserProfileDTO& profile, const std::string& sessionToken) {
    json data = loadAccountsPageData(profile);
    GetSavingsListRequest request;
    request.session_token = sessionToken;
    request.target_user_id = profile.user_id;
    request.status_filter = "ALL";
    const GetSavingsListResult result = m_savingsService.getSavingsList(request);
    if (!result.success) {
        throw std::runtime_error(result.message.empty() ? "Không thể tải danh sách sổ tiết kiệm." : result.message);
    }
    data["savings"] = result.items;
    return data;
}

json PageDataLoaderService::loadTransactionsPageData(const UserProfileDTO& profile, const std::string& sessionToken) {
    const GetAccountsResult accounts = m_accountService.getAccounts(profile.user_id, profile.user_id);
    if (!accounts.success) {
        throw std::runtime_error(accounts.message.empty() ? "Không thể tải danh sách tài khoản." : accounts.message);
    }

    std::string accountId = profile.primary_account_id;
    const auto primary = std::find_if(accounts.accounts.begin(), accounts.accounts.end(),
        [&](const AccountDTO& account) { return account.account_id == accountId; });
    if (primary == accounts.accounts.end()) {
        accountId = accounts.accounts.empty() ? "" : accounts.accounts.front().account_id;
    }

    json transactions = json::array();
    if (!accountId.empty()) {
        const TransactionHistoryResult history =
            m_accountService.getTransactionHistory(profile.user_id, accountId, 50, 1);
        if (!history.success) {
            throw std::runtime_error(history.message.empty() ? "Không thể tải lịch sử giao dịch." : history.message);
        }
        transactions = history.items;
    }

    return {
        {"profile", profile},
        {"accounts", accounts.accounts},
        {"transactions", transactions}
    };
}

json PageDataLoaderService::loadLoanPageData(const UserProfileDTO& profile, const std::string& sessionToken) {
    json data = loadAccountsPageData(profile);
    data["loanApps"] = json::array();
    data["activeLoans"] = json::array();
    data["loanTerms"] = m_loanService.getAvailableTerms();

    const std::vector<LoanModel> loans = m_loanService.getLoansForSession(sessionToken);
    for (const LoanModel& loan : loans) {
        const int stage = loanStage(loan.getStatus());
        const std::string method = loan.getCalcMethod() == "FLAT" ? "FIXED" : loan.getCalcMethod();
        const LoanDocumentsResult documents = m_loanService.getDocuments(sessionToken, loan.getLoanId());
        if (!documents.success) throw std::runtime_error(documents.message);
        json docs = json::array();
        for (const auto& document : documents.documents) {
            const std::map<std::string, std::string> names = {
                {"cccd", "CCCD gắn chip (2 mặt)"}, {"income", "Sao kê lương / HĐLĐ 3 tháng gần nhất"},
                {"collateral", "Giấy tờ tài sản đảm bảo"}, {"address", "Xác nhận nơi cư trú"}
            };
            const std::string status = document.status == "APPROVED" ? "DONE" :
                (document.status == "PENDING_REVIEW" ? "REVIEW" :
                 (document.status == "REJECTED" ? "RESUBMIT" : "MISSING"));
            const std::string documentType = document.document_type;
            const std::string documentName = documentType.rfind("CUSTOM:", 0) == 0
                ? documentType.substr(7)
                : (names.count(documentType) ? names.at(documentType) : documentType);
            docs.push_back({{"id", document.document_type}, {"icon", "file"},
                {"name", documentName},
                {"status", status}, {"hint", document.review_note.empty() ? "Chưa có tệp" : document.review_note},
                {"file", document.status == "MISSING" ? "" : document.file_name},
                {"mime", document.mime_type}, {"size", document.file_size},
                {"at", document.submitted_at}, {"url", ""}, {"document_id", document.document_id}});
        }
        data["loanApps"].push_back({
            {"id", loan.getLoanId()},
            {"who", profile.fullname},
            {"phone", profile.phone},
            {"type", loanType(loan)},
            {"amt", loan.getOriginalAmountVnd()},
            {"term", loan.getTermMonths()},
            {"rate", loan.getAnnualInterestRate()},
            {"status", loan.getStatus()},
            {"method", method},
            {"purpose", loan.getPurpose()},
            {"collateral", loan.getCollateralInfo()},
            {"income", loan.getMonthlyIncomeVnd()},
            {"stage", stage},
            {"needInfo", false},
            {"resubmitted", false},
            {"result", nullptr},
            {"docs", docs}
        });

        if (loan.getStatus() == "DISBURSED" && loan.getRemainingDebtVnd() > 0) {
            const LoanSchedulesResult scheduleResult = m_loanService.getSchedules(sessionToken, loan.getLoanId());
            if (!scheduleResult.success) throw std::runtime_error(scheduleResult.message);
            json scheduleRows = json::array();
            const LoanScheduleDTO* nextSchedule = nullptr;
            int paidCount = 0;
            for (const auto& schedule : scheduleResult.schedules) {
                scheduleRows.push_back(schedule);
                if (schedule.status == "PAID") ++paidCount;
                else if (!nextSchedule) nextSchedule = &schedule;
            }
            const int left = std::max(0, loan.getTermMonths() - paidCount);
            data["activeLoans"].push_back({
                {"id", loan.getLoanId()},
                {"type", loanType(loan)},
                {"principal", loan.getOriginalAmountVnd()},
                {"outstanding", loan.getRemainingDebtVnd()},
                {"rate", loan.getAnnualInterestRate()},
                {"total", loan.getTermMonths()},
                {"left", left},
                {"pay", nextSchedule ? nextSchedule->total_due_vnd : 0},
                {"due", nextSchedule ? nextSchedule->due_date : ""},
                {"nextScheduleId", nextSchedule ? nextSchedule->schedule_id : ""},
                {"accountId", loan.getDisbursedAccountId()},
                {"schedules", scheduleRows}
            });
        }
    }
    return data;
}

} // namespace Banking
