#ifndef AUTH_SECURITY_HPP
#define AUTH_SECURITY_HPP

#include<string>
#include <sstream>
#include <iomanip>
#include <random>
#include <core/sha256.hpp>

class AuthSecurity {
    public:
    static    std::string generateSalt(size_t length = 16);
    static    std::string hashPin ( const std::string&PIN );
    static    std::string generateOTP(size_t length = 6);
    static    std::string generateToken(size_t length = 32);
    static    std::string generateNumericCode(size_t length = 20);
    static    std::string hashPasswordPBKDF2(const std::string& password, const std::string& salt);
    static    bool verifyPassword(const std::string& rawPassword, const std::string& storedHash, const std::string& salt);
};
std::string vectorToHexString(const std::vector<uint8_t>& bytes);

#endif
