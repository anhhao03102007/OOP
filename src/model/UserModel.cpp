#include "model/UserModel.hpp"
#include <core/AuthSecurity.hpp>
#include <utility>

namespace Banking {

// =========================================================================
// 1. BASE CLASS: User
// =========================================================================
User::User(std::string userId, std::string username, std::string passwordHash,
           std::string salt, std::string fullName, std::string email,
           std::string phone, std::string cccd, std::string status)
    : m_userId(std::move(userId)), m_username(std::move(username)),
      m_passwordHash(std::move(passwordHash)), m_salt(std::move(salt)),
      m_fullName(std::move(fullName)), m_email(std::move(email)),
      m_phone(std::move(phone)), m_cccd(std::move(cccd)),
      m_status(std::move(status)) {}

bool User::hasPermission(Permission perm) const {
    return m_permissions.find(perm) != m_permissions.end();
}

bool User::isBlocked() const { return m_status == "BLOCKED"; }
bool User::isActive() const { return m_status == "ACTIVE"; }

const std::string& User::getUserId() const { return m_userId; }
const std::string& User::getUsername() const { return m_username; }
const std::string& User::getPasswordHash() const { return m_passwordHash; }
const std::string& User::getSalt() const { return m_salt; }
const std::string& User::getFullName() const { return m_fullName; }
const std::string& User::getEmail() const { return m_email; }
const std::string& User::getPhone() const { return m_phone; }
const std::string& User::getCCCD() const { return m_cccd; }

void User::setPinHash(std::string pinHash) { m_pinHash = std::move(pinHash); }
const std::string& User::getPinHash() const { return m_pinHash; }

bool User::verifyPin(const std::string& rawPin) const {
    if (rawPin.empty() || m_pinHash.empty()) return false;
    return AuthSecurity::hashPin(rawPin) == m_pinHash;
}

int User::getFailedAttempts() const { return m_failedAttempts; }
void User::setFailedAttempts(int attempts) { m_failedAttempts = attempts; }

// =========================================================================
// 2. DERIVED CLASS: CustomerUser
// =========================================================================
CustomerUser::CustomerUser(std::string userId, std::string username, std::string passwordHash,
                           std::string salt, std::string fullName, std::string email,
                           std::string phone, std::string cccd, std::string status)
    : User(std::move(userId), std::move(username), std::move(passwordHash),
           std::move(salt), std::move(fullName), std::move(email),
           std::move(phone), std::move(cccd), std::move(status)) {
    m_permissions = {
        Permission::SELF_VIEW_PROFILE,
        Permission::SELF_TRANSFER,
        Permission::SELF_OPEN_SAVINGS,
        Permission::SELF_APPLY_LOAN,
        Permission::SELF_LOCK_ACCOUNT
    };
}

std::string CustomerUser::getRole() const { return "CUSTOMER"; }
bool CustomerUser::canBeBlocked() const { return true; }

// =========================================================================
// 3. DERIVED CLASS: TellerUser
// =========================================================================
TellerUser::TellerUser(std::string userId, std::string username, std::string passwordHash,
                       std::string salt, std::string fullName, std::string email,
                       std::string phone, std::string cccd, std::string status)
    : User(std::move(userId), std::move(username), std::move(passwordHash),
           std::move(salt), std::move(fullName), std::move(email),
           std::move(phone), std::move(cccd), std::move(status)) {
    m_permissions = {
        Permission::SELF_VIEW_PROFILE,
        Permission::TELLER_COUNTER_CASH,
        Permission::TELLER_SEARCH_CUSTOMER,
        Permission::TELLER_OPEN_ACCOUNT,
        Permission::TELLER_TOGGLE_USER_LOCK
    };
}

std::string TellerUser::getRole() const { return "TELLER"; }
bool TellerUser::canBeBlocked() const { return true; }

// =========================================================================
// 4. DERIVED CLASS: AdminUser
// =========================================================================
AdminUser::AdminUser(std::string userId, std::string username, std::string passwordHash,
                     std::string salt, std::string fullName, std::string email,
                     std::string phone, std::string cccd, std::string status)
    : User(std::move(userId), std::move(username), std::move(passwordHash),
           std::move(salt), std::move(fullName), std::move(email),
           std::move(phone), std::move(cccd), std::move(status)) {
    m_permissions = {
        Permission::SELF_VIEW_PROFILE,
        Permission::TELLER_COUNTER_CASH,
        Permission::TELLER_SEARCH_CUSTOMER,
        Permission::TELLER_OPEN_ACCOUNT,
        Permission::TELLER_TOGGLE_USER_LOCK,
        Permission::ADMIN_MANAGE_STAFF,
        Permission::ADMIN_APPROVE_LOAN_LARGE,
        Permission::ADMIN_VIEW_AUDIT_LOG,
        Permission::ADMIN_SYSTEM_CONFIG
    };
}

std::string AdminUser::getRole() const { return "ADMIN"; }
bool AdminUser::canBeBlocked() const { return false; }

} // namespace Banking
