#ifndef LOAN_REPOSITORY_HPP
#define LOAN_REPOSITORY_HPP

#include <optional>
#include <string>
#include <vector>
#include <core/json.hpp>
#include "core/TursoDB.hpp"
#include "model/LoansModel.hpp"

namespace Banking {

using json = nlohmann::json;

class LoanRepository {
private:
    TursoDB& m_tursoDb;

    static std::string extractCell(const json& cell);

public:
    explicit LoanRepository(TursoDB& tursoDb);

    // 1. Tra cứu khoản vay theo Loan ID
    std::optional<LoanModel> findById(const std::string& loanId);

    // 2. Tra cứu toàn bộ danh sách khoản vay của một User
    std::vector<LoanModel> findByUserId(const std::string& userId);
    std::vector<LoanModel> findAll();

    // 3. Cập nhật trạng thái và thông tin phê duyệt khoản vay
    bool updateLoanStatus(const std::string& loanId, const std::string& status, 
                          const std::string& approvedBy = "", const std::string& disbursedAccountId = "");

    // 4. Cập nhật số dư nợ còn lại (Dùng cho gạch nợ / trả góp)
    bool updateRemainingDebt(const std::string& loanId, int64_t newRemainingDebtVnd);
};

} // namespace Banking

#endif // LOAN_REPOSITORY_HPP