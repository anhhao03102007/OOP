#ifndef USER_REPOSITORY_HPP
#define USER_REPOSITORY_HPP

#include <string>
#include <vector>
#include <memory>
#include <core/json.hpp>
#include <core/TursoDB.hpp>
#include <model/UserModel.hpp>

namespace Banking {

using json = nlohmann::json;

class UserRepository {
private:
    TursoDB& m_tursoDb;

    static std::string extractCell(const json& cell);
    static std::shared_ptr<User> mapRowToUser(const json& r);

public:
    explicit UserRepository(TursoDB& tursoDb);

    // 1. Tra cứu thông tin
    std::shared_ptr<User> findById(const std::string& userId);
    std::shared_ptr<User> findByAccountId(const std::string& accountId);
    std::vector<std::shared_ptr<User>> searchCustomers(const std::string& query);
    std::vector<std::shared_ptr<User>> findStaffMembers();
    bool createStaff(const std::string& userId, const std::string& username,
                     const std::string& passwordHash, const std::string& salt,
                     const std::string& fullname, const std::string& email,
                     const std::string& phone, const std::string& cccd,
                     const std::string& role);

    // 2. Cập nhật thông tin cá nhân / Bảo mật
    bool updateProfile(const std::string& userId, const std::string& fullname, const std::string& email, const std::string& phone);
    bool updatePassword(const std::string& userId, const std::string& newPasswordHash, const std::string& newSalt);
    bool updatePin(const std::string& userId, const std::string& pinHash);
    bool updateStatus(const std::string& userId, const std::string& newStatus);
    
    // 3. Helper lấy mã PIN Hash để verify
    std::string getPinHash(const std::string& userId);
};

} // namespace Banking

#endif // USER_REPOSITORY_HPP
