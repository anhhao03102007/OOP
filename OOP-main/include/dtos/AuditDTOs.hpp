#ifndef AUDIT_DTOS_HPP
#define AUDIT_DTOS_HPP

#include <string>
#include <vector>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

// 1. DTO đại diện cho 1 dòng log
struct AuditLogItemDTO {
    std::string log_id;
    std::string operator_id;
    std::string action;
    std::string target_id;
    std::string details;
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AuditLogItemDTO, log_id, operator_id, action, target_id, details, created_at)
};

// 2. Yêu cầu tra cứu Audit Log phân trang từ Frontend Admin
struct GetAuditLogsRequest {
    std::string session_token;
    int page = 1;
    int page_size = 20;
    std::string operator_filter = ""; // Lọc theo ID người thực hiện (nếu có)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GetAuditLogsRequest, session_token, page, page_size, operator_filter)
};

// 3. Kết quả trả về danh sách Audit Log
struct GetAuditLogsResult {
    bool success = false;
    std::string message;
    int current_page = 1;
    int total_pages = 1;
    int total_items = 0;
    std::vector<AuditLogItemDTO> items;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GetAuditLogsResult, success, message, current_page, total_pages, total_items, items)
};

struct GetAuditLogByIdResult {
    bool success = false;
    std::string message;
    AuditLogItemDTO item;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GetAuditLogByIdResult, success, message, item)
};

} // namespace Banking

#endif // AUDIT_DTOS_HPP
