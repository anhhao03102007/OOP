#include "services/AuditService.hpp"

namespace Banking {

AuditService::AuditService(AuditRepository& auditRepo, UserRepository& userRepo, SessionManager& sessionManager)
    : m_auditRepo(auditRepo), m_userRepo(userRepo), m_sessionManager(sessionManager) {}

GetAuditLogsResult AuditService::getLogs(const GetAuditLogsRequest& req) {
    GetAuditLogsResult res;

    // 1. Xác thực phiên làm việc
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc đã hết hạn hoặc không hợp lệ!";
        return res;
    }

    // 2. Kiểm tra RBAC: Chỉ Admin mới có quyền xem Audit Log
    auto user = m_userRepo.findById(sessOpt->user_id);
    if (!user || !user->hasPermission(Permission::ADMIN_VIEW_AUDIT_LOG)) {
        res.message = "BẢO MẬT: Bạn không có quyền truy cập nhật ký hệ thống (Audit Log)!";
        return res;
    }

    // 3. Truy vấn phân trang từ Repository
    return m_auditRepo.getLogsPaginated(req.page, req.page_size, req.operator_filter);
}

GetAuditLogByIdResult AuditService::getLogById(const std::string& sessionToken, const std::string& logId) {
    GetAuditLogByIdResult res;
    const auto session = m_sessionManager.getSession(sessionToken);
    if (!session) {
        res.message = "Phiên làm việc đã hết hạn hoặc không hợp lệ!";
        return res;
    }

    const auto user = m_userRepo.findById(session->user_id);
    const bool isAdmin = user && user->hasPermission(Permission::ADMIN_VIEW_AUDIT_LOG);
    if (!user || (!isAdmin && user->getRole() != "TELLER")) {
        res.message = "BẢO MẬT: Chỉ Admin hoặc giao dịch viên mới được tra cứu mã thực thi.";
        return res;
    }
    if (logId.empty()) {
        res.message = "Cần cung cấp mã tra cứu.";
        return res;
    }

    const auto item = m_auditRepo.getLogById(logId);
    if (!item) {
        res.message = "Không tìm thấy nhật ký theo mã này.";
        return res;
    }
    if (!isAdmin && (item->operator_id != session->user_id ||
        (item->action != "DEPOSIT_COUNTER_SUCCESS" && item->action != "DEPOSIT_COUNTER_FAILED" &&
         item->action != "COUNTER_WITHDRAW_SUCCESS" && item->action != "COUNTER_WITHDRAW_FAILED"))) {
        res.message = "Mã này không thuộc giao dịch quầy của bạn.";
        return res;
    }
    res.success = true;
    res.message = "Tra cứu nhật ký thành công.";
    res.item = *item;
    return res;
}

} // namespace Banking
