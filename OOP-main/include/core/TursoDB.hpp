#ifndef TURSO_DB_HPP
#define TURSO_DB_HPP

#include <string>
#include <utility>
#include <vector>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

class TursoDB {
private:
    std::string m_dbUrl;
    std::string m_authToken;

public:
    TursoDB(std::string dbUrl, std::string authToken);

    // Thực thi 1 câu lệnh đơn có tham số (Parameterized Query)
    json execute(const std::string& sql, const json& args = json::array());

    // Thực thi danh sách câu lệnh trong 1 Transaction (Atomic Batch Execution)
    json executeBatch(const std::vector<std::pair<std::string, json>>& statements);

    bool checkConnection(std::string& error);

};

} // namespace Banking

#endif // TURSO_DB_HPP
