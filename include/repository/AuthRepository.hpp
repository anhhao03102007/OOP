#ifndef AUTH_REPOSITORY_HPP
#define AUTH_REPOSITORY_HPP

#include <string>
#include <optional>
#include <memory>
#include <core/json.hpp>
#include "dtos/AuthDTOs.hpp"
#include "model/UserModel.hpp"
#include "core/TursoDB.hpp"

namespace Banking {

using json = nlohmann::json;

class AuthRepository {
private:
    TursoDB& m_tursoDb;

    static std::string extractCell(const json& cell);
    static std::shared_ptr<User> mapRowToUser(const json& r);

public:
    explicit AuthRepository(TursoDB& tursoDb);

    // 1. Tra cứu người dùng qua Username / Email / Phone / CCCD
    std::shared_ptr<User> findByIdentifier(const std::string& identifier);

    // 2. Kiểm tra tồn tại trùng lặp thông tin khi đăng ký
    bool existsByIdentifier(const std::string& val);

    // 3. Lấy số tài khoản thanh toán mặc định của User
    std::string getPrimaryAccountId(const std::string& userId);

    // 4. Tạo người dùng khách hàng mới
    bool createCustomer(const std::string& userId, const std::string& username, const std::string& passHash,
                        const std::string& salt, const std::string& fullname, const std::string& email,
                        const std::string& cccd, const std::string& phone);

    bool createCustomerWithCheckingAccount(const std::string& userId, const std::string& username,
                                           const std::string& passHash, const std::string& salt,
                                           const std::string& fullname, const std::string& email,
                                           const std::string& cccd, const std::string& phone,
                                           const std::string& accountId);

    // 5. Cập nhật mật khẩu băm & Salt mới (Dùng cho Quên mật khẩu)
    bool updatePassword(const std::string& userId, const std::string& newPassHash, const std::string& newSalt);

    // 6. Quản lý đếm đăng nhập sai & Khóa tài khoản
    void updateFailedAttempts(const std::string& userId, int count);
    void lockUserTemporarily(const std::string& userId, int minutes);
    bool releaseExpiredTemporaryLock(const std::string& userId);
    void resetFailedAttempts(const std::string& userId);

    int64_t getBalance(const std::string& accountId);
};

} // namespace Banking

#endif // AUTH_REPOSITORY_HPP
