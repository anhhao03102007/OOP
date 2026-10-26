#ifndef SAVINGS_REPOSITORY_HPP
#define SAVINGS_REPOSITORY_HPP

#include <string>
#include <optional>
#include <core/json.hpp>
#include "core/TursoDB.hpp"
#include "model/AccountModel.hpp"
#include "model/SavingsBookModel.hpp"

namespace Banking {

using json = nlohmann::json;

class SavingsRepository {
private:
    TursoDB& m_tursoDb;

    // Các hàm trợ giúp nội bộ để parse dữ liệu JSON từ TursoDB
    static std::string extractCell(const json& cell);
    static TermSavingsAccount mapRowToModel(const json& r);

public:
    explicit SavingsRepository(TursoDB& tursoDb);

    // 1. Tạo mới sổ tiết kiệm vào CSDL
    bool openSavings(const std::string& savingsId, const std::string& userId,
                     const std::string& sourceAccountId, int64_t principalVnd,
                     double interestRate, int termMonths, const std::string& maturityOption,
                     const std::string& startDate, const std::string& maturityDate,
                     const std::string& createdAt);

    // 2. Tìm sổ tiết kiệm theo Mã sổ (savings_id)
    std::optional<TermSavingsAccount> findById(const std::string& savingsId);

    // 3. Lấy tất cả sổ tiết kiệm thuộc về một Khách hàng
    SavingsBook findSavingsBookByUserId(const std::string& userId, const std::string& statusFilter = "ALL");

    // 4. Cập nhật trạng thái sổ tiết kiệm (VD: ACTIVE, CLOSED)
    bool updateStatus(const std::string& savingsId, const std::string& newStatus);

    // 5. Cập nhật phương thức đáo hạn (Maturity Option)
    bool updateMaturityOption(const std::string& savingsId, const std::string& newOption);

    // 6. Đếm tổng số sổ tiết kiệm đang hoạt động của một khách hàng
    int countActiveSavings(const std::string& userId);
};

} // namespace Banking

#endif // SAVINGS_REPOSITORY_HPP
