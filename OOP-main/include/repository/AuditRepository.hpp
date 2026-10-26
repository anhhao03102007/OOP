#ifndef AUDIT_REPOSITORY_HPP
#define AUDIT_REPOSITORY_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <core/json.hpp>
#include "core/TursoDB.hpp"
#include "dtos/AuditDTOs.hpp"

namespace Banking {

class AuditRepository {
private:
    TursoDB& m_tursoDb;

    // Trích xuất giá trị ô dữ liệu từ JSON response của TursoDB
    static std::string extractCell(const nlohmann::json& cell);

    // Lấy thời gian ISO8601 UTC hiện tại
    static std::string getCurrentTimestamp();

public:
    explicit AuditRepository(TursoDB& tursoDb);

    // 1. Ghi nhận vết hành động (Append-Only Transaction)
    bool logAction(const std::string& operatorId, const std::string& action,
                   const std::string& targetId, const std::string& details);

    // Ghi audit với ID do nghiệp vụ cung cấp để có thể tra cứu chính xác theo mã thực thi.
    bool logActionWithId(const std::string& logId, const std::string& operatorId, const std::string& action,
                        const std::string& targetId, const std::string& details);

    // 2. Tra cứu danh sách Audit Log có Phân Trang dành cho Admin (LIMIT / OFFSET)
    GetAuditLogsResult getLogsPaginated(int page, int pageSize, const std::string& operatorFilter = "");
    std::optional<AuditLogItemDTO> getLogById(const std::string& logId);
};

} // namespace Banking

#endif // AUDIT_REPOSITORY_HPP
