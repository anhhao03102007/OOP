#include<iostream>
#include <core/AuthSecurity.hpp>
#include <QRandomGenerator>
#include <cstdint>
using namespace YukiWorkshop::Crypto;

namespace {
std::string randomChars(size_t length, const char* charset, size_t charsetSize) {
    auto* rng = QRandomGenerator::system();
    std::string out; out.reserve(length);
    for (size_t i = 0; i < length; ++i) out += charset[rng->bounded(static_cast<quint32>(charsetSize))];
    return out;
}
std::string hmacSha256(const std::string& key, const std::string& data) {
    SHA256_HMAC hmac(key); hmac.update(data); return static_cast<std::string>(hmac.finalize());
}
std::string pbkdf2(const std::string& password, const std::string& salt, uint32_t rounds) {
    std::string block = salt + std::string("\0\0\0\1", 4);
    std::string u = hmacSha256(password, block), out = u;
    for (uint32_t i = 1; i < rounds; ++i) {
        u = hmacSha256(password, u);
        for (size_t j = 0; j < out.size(); ++j) out[j] ^= u[j];
    }
    return vectorToHexString(std::vector<uint8_t>(out.begin(), out.end()));
}
bool constantTimeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0; for (size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}
}

std::string AuthSecurity::generateSalt(size_t length) {
        static const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        return randomChars(length, charset, sizeof(charset) - 1);
    }
std::string vectorToHexString(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (uint8_t b : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return ss.str();
}
std::string AuthSecurity::hashPin(const std::string& PIN ){
    SHA256 s;
    s.update(PIN);
    std::string hash_str = vectorToHexString(s.finalize());
    return hash_str;
}
std::string AuthSecurity::generateOTP(size_t length) {
        static constexpr char digits[] = "0123456789";
        return randomChars(length, digits, 10);
    }

std::string AuthSecurity::generateToken(size_t length) {
        static constexpr char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        return randomChars(length, charset, sizeof(charset) - 1);
    }
std::string AuthSecurity::generateNumericCode(size_t length) {
        static constexpr char digits[] = "0123456789";
        return randomChars(length, digits, sizeof(digits) - 1);
    }
std::string AuthSecurity::hashPasswordPBKDF2(const std::string& password, const std::string& salt){
    return "pbkdf2_sha256$120000$" + pbkdf2(password, salt, 120000);
}
bool AuthSecurity::verifyPassword(const std::string& rawPassword, 
                                  const std::string& storedHash, 
                                  const std::string& salt) {
    if (rawPassword.empty() || storedHash.empty() || salt.empty()) {
        return false;
    }

    constexpr const char* prefix = "pbkdf2_sha256$120000$";
    if (storedHash.rfind(prefix, 0) == 0)
        return constantTimeEqual(hashPasswordPBKDF2(rawPassword, salt).substr(21), storedHash.substr(21));
    // Legacy SHA-256 hashes are accepted for one successful login, then upgraded by AuthService.
    SHA256 legacy; legacy.update(rawPassword + salt);
    return constantTimeEqual(vectorToHexString(legacy.finalize()), storedHash);
}
