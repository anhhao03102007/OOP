#ifndef USER_DTOS_HPP
#define USER_DTOS_HPP

#include <string>
#include <vector>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

struct UserProfileDTO {
    std::string user_id;
    std::string username;
    std::string fullname;
    std::string email;
    std::string phone;
    std::string cccd;
    std::string role;
    std::string status;
    std::string primary_account_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UserProfileDTO, user_id, username, fullname, email, phone, cccd, role, status, primary_account_id)
};

struct UpdateProfileRequest {
    std::string session_token;
    std::string fullname;
    std::string email;
    std::string phone;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UpdateProfileRequest, session_token, fullname, email, phone)
};

struct ChangePasswordRequest {
    std::string session_token;
    std::string old_password;
    std::string new_password;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ChangePasswordRequest, session_token, old_password, new_password)
};

struct ChangePinRequest {
    std::string session_token;
    std::string old_pin;
    std::string new_pin;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ChangePinRequest, session_token, old_pin, new_pin)
};

struct CustomerSearchRequest {
    std::string session_token;
    std::string query;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CustomerSearchRequest, session_token, query)
};

struct UserOperationResult {
    bool success = false;
    std::string message;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UserOperationResult, success, message)
};

} // namespace Banking

#endif // USER_DTOS_HPP