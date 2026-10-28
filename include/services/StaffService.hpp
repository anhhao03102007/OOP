#ifndef STAFF_SERVICE_HPP
#define STAFF_SERVICE_HPP

#include <core/SessionManager.hpp>
#include <core/TursoDB.hpp>
#include <dtos/StaffDTOs.hpp>
#include <repository/AuditRepository.hpp>
#include <repository/UserRepository.hpp>

namespace Banking {

class StaffService {
private:
    TursoDB& m_database;
    UserRepository& m_userRepository;
    AuditRepository& m_auditRepository;
    SessionManager& m_sessionManager;

    bool hasAdminPermission(const std::string& sessionToken, Permission permission,
                            std::string* operatorId = nullptr);
    bool isAdmin(const std::string& sessionToken, std::string* operatorId = nullptr);

public:
    StaffService(TursoDB& database, UserRepository& userRepository,
                 AuditRepository& auditRepository, SessionManager& sessionManager);

    SystemConfigDTO getSystemConfig(const GetSystemConfigRequest& request);
    StaffOperationResult updateSystemConfig(const UpdateSystemConfigRequest& request);
    StaffMembersResult getStaffMembers(const std::string& sessionToken);
    StaffOperationResult createStaff(const CreateStaffRequest& request);
    DashboardStatsDTO getDashboardStats(const std::string& sessionToken);
};

} // namespace Banking

#endif // STAFF_SERVICE_HPP
