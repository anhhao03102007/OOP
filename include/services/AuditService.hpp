#ifndef AUDIT_SERVICE_HPP
#define AUDIT_SERVICE_HPP

#include <string>
#include <memory>
#include "dtos/AuditDTOs.hpp"
#include "repository/AuditRepository.hpp"
#include "repository/UserRepository.hpp"
#include "core/SessionManager.hpp"

namespace Banking {

class AuditService {
private:
    AuditRepository& m_auditRepo;
    UserRepository&  m_userRepo;
    SessionManager&  m_sessionManager;

public:
    AuditService(AuditRepository& auditRepo, UserRepository& userRepo, SessionManager& sessionManager);

    // Tra cứu danh sách nhật ký Audit (Yêu cầu quyền Admin)
    GetAuditLogsResult getLogs(const GetAuditLogsRequest& req);
    GetAuditLogByIdResult getLogById(const std::string& sessionToken, const std::string& logId);
};

} // namespace Banking

#endif // AUDIT_SERVICE_HPP
