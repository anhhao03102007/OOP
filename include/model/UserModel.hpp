#ifndef USER_MODEL_HPP
#define USER_MODEL_HPP

#include <string>
#include <unordered_set>

namespace Banking {

// Danh mục Quyền hạn trong hệ thống
enum class Permission {
    // Quyền Khách hàng
    SELF_VIEW_PROFILE,
    SELF_TRANSFER,
    SELF_OPEN_SAVINGS,
    SELF_APPLY_LOAN,
    SELF_LOCK_ACCOUNT,
    
    // Quyền Giao dịch viên (Teller)
    TELLER_COUNTER_CASH,       // Nạp/rút tiền quầy
    TELLER_SEARCH_CUSTOMER,    // Tra cứu thông tin khách hàng
    TELLER_OPEN_ACCOUNT,       // Mở tài khoản cho khách
    TELLER_TOGGLE_USER_LOCK,   // Khóa/mở khóa tài khoản khách
    
    // Quyền Quản trị viên (Admin)
    ADMIN_MANAGE_STAFF,        // Tạo/quản lý tài khoản nhân viên
    ADMIN_APPROVE_LOAN_LARGE,  // Duyệt vay > 100 Triệu VNĐ
    ADMIN_VIEW_AUDIT_LOG,      // Xem nhật ký hệ thống
    ADMIN_SYSTEM_CONFIG        // Cấu hình phí & lãi suất
};

// =========================================================================
// 1. BASE CLASS: User (Trừu tượng)
// =========================================================================
class User {
protected:
    std::string m_userId;
    std::string m_username;
    std::string m_passwordHash;
    std::string m_salt;
    std::string m_pinHash;
    std::string m_fullName;
    std::string m_email;
    std::string m_phone;
    std::string m_cccd;
    std::string m_status; // "ACTIVE", "BLOCKED"
    int         m_failedAttempts = 0; // Số lần đăng nhập sai từ CSDL
    std::unordered_set<Permission> m_permissions;

public:
    User(std::string userId, std::string username, std::string passwordHash,
         std::string salt, std::string fullName, std::string email,
         std::string phone, std::string cccd, std::string status);

    virtual ~User() = default;

    // Fast Permission Guard
    [[nodiscard]] bool hasPermission(Permission perm) const;
    [[nodiscard]] bool isBlocked() const;
    [[nodiscard]] bool isActive() const;

    // Pure Virtual Functions
    [[nodiscard]] virtual std::string getRole() const = 0;
    [[nodiscard]] virtual bool canBeBlocked() const = 0; // Quy tắc an ninh

    // Getters
    [[nodiscard]] const std::string& getUserId() const;
    [[nodiscard]] const std::string& getUsername() const;
    [[nodiscard]] const std::string& getPasswordHash() const;
    [[nodiscard]] const std::string& getSalt() const;
    [[nodiscard]] const std::string& getFullName() const;
    [[nodiscard]] const std::string& getEmail() const;
    [[nodiscard]] const std::string& getPhone() const;
    [[nodiscard]] const std::string& getCCCD() const;

    void setPinHash(std::string pinHash);
    [[nodiscard]] const std::string& getPinHash() const;

    // Phương thức xác thực mã PIN 6 số
    [[nodiscard]] bool verifyPin(const std::string& rawPin) const;
    [[nodiscard]] int getFailedAttempts() const;
    void setFailedAttempts(int attempts);
};

// =========================================================================
// 2. DERIVED CLASS: CustomerUser (Khách hàng)
// =========================================================================
class CustomerUser : public User {
public:
    CustomerUser(std::string userId, std::string username, std::string passwordHash,
                 std::string salt, std::string fullName, std::string email,
                 std::string phone, std::string cccd, std::string status);

    [[nodiscard]] std::string getRole() const override;
    [[nodiscard]] bool canBeBlocked() const override;
};

// =========================================================================
// 3. DERIVED CLASS: TellerUser (Giao dịch viên)
// =========================================================================
class TellerUser : public User {
public:
    TellerUser(std::string userId, std::string username, std::string passwordHash,
               std::string salt, std::string fullName, std::string email,
               std::string phone, std::string cccd, std::string status);

    [[nodiscard]] std::string getRole() const override;
    [[nodiscard]] bool canBeBlocked() const override;
};

// =========================================================================
// 4. DERIVED CLASS: AdminUser (Quản trị viên)
// =========================================================================
class AdminUser : public User {
public:
    AdminUser(std::string userId, std::string username, std::string passwordHash,
              std::string salt, std::string fullName, std::string email,
              std::string phone, std::string cccd, std::string status);

    [[nodiscard]] std::string getRole() const override;
    [[nodiscard]] bool canBeBlocked() const override;
};

} // namespace Banking

#endif // USER_MODEL_HPP
