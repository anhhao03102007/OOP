#include <services/StaffService.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <utility>
#include <core/AuthSecurity.hpp>

namespace Banking {
namespace {

using json = nlohmann::json;

std::string cellText(const json& cell) {
    if (!cell.is_object() || !cell.contains("value") || cell["value"].is_null()) return {};
    const auto& value = cell["value"];
    return value.is_string() ? value.get<std::string>() : value.dump();
}

bool hasDatabaseError(const json& response) {
    if (!response.contains("results") || !response["results"].is_array()) return true;
    return std::any_of(response["results"].begin(), response["results"].end(), [](const json& item) {
        return item.is_object() && item.value("type", "") == "error";
    });
}

std::vector<LoanTermConfigDTO> defaultLoanTerms() {
    return {{3, true, 10.5}, {6, true, 11.0}, {12, true, 12.0}};
}

bool hasOnlyUsernameCharacters(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '.' || c == '-';
    });
}

bool allDigits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

bool validEmail(const std::string& email) {
    const auto at = email.find('@');
    if (at == std::string::npos || at == 0 || at != email.rfind('@') || email.size() > 254) return false;
    const std::string local = email.substr(0, at);
    const std::string domain = email.substr(at + 1);
    if (local.size() > 64 || local.front() == '.' || local.back() == '.' || local.find("..") != std::string::npos ||
        domain.size() < 3 || domain.front() == '.' || domain.back() == '.' || domain.find('.') == std::string::npos) return false;
    if (!std::all_of(local.begin(), local.end(), [](unsigned char c) {
        return std::isalnum(c) || std::string(".!#$%&'*+/=?^_`{|}~-").find(static_cast<char>(c)) != std::string::npos;
    })) return false;
    bool labelStart = true;
    for (size_t i = 0; i < domain.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(domain[i]);
        if (c == '.') {
            if (labelStart || domain[i - 1] == '-') return false;
            labelStart = true;
        } else if (std::isalnum(c)) {
            labelStart = false;
        } else if (c == '-' && !labelStart && i + 1 < domain.size() && domain[i + 1] != '.') {
            continue;
        } else {
            return false;
        }
    }
    return !labelStart && domain.back() != '-';
}

} // namespace

StaffService::StaffService(TursoDB& database, UserRepository& userRepository,
                           AuditRepository& auditRepository, SessionManager& sessionManager)
    : m_database(database), m_userRepository(userRepository),
      m_auditRepository(auditRepository), m_sessionManager(sessionManager) {}

bool StaffService::hasAdminPermission(const std::string& sessionToken, Permission permission,
                                      std::string* operatorId) {
    const auto session = m_sessionManager.getSession(sessionToken);
    if (!session || session->role != "ADMIN") return false;

    const auto user = m_userRepository.findById(session->user_id);
    if (!user || !user->isActive() || !user->hasPermission(permission)) return false;
    if (operatorId) *operatorId = session->user_id;
    return true;
}

bool StaffService::isAdmin(const std::string& sessionToken, std::string* operatorId) {
    return hasAdminPermission(sessionToken, Permission::ADMIN_SYSTEM_CONFIG, operatorId);
}

StaffMembersResult StaffService::getStaffMembers(const std::string& sessionToken) {
    StaffMembersResult result;
    if (!hasAdminPermission(sessionToken, Permission::ADMIN_MANAGE_STAFF)) {
        result.message = "BẢO MẬT: Bạn không có quyền quản lý nhân viên.";
        return result;
    }

    for (const auto& user : m_userRepository.findStaffMembers()) {
        if (!user) continue;
        result.staff.push_back({user->getUserId(), user->getUsername(), user->getFullName(),
                                user->getEmail(), user->getPhone(), user->getRole(),
                                user->isActive() ? "ACTIVE" : "BLOCKED"});
    }
    result.success = true;
    result.message = "Đã tải danh sách nhân viên.";
    return result;
}

DashboardStatsDTO StaffService::getDashboardStats(const std::string& sessionToken) {
    DashboardStatsDTO result;
    const auto session = m_sessionManager.getSession(sessionToken);
    const auto user = session ? m_userRepository.findById(session->user_id) : nullptr;
    if (!session || !user || !user->isActive() ||
        (session->role != "ADMIN" && session->role != "TELLER")) {
        result.message = "Bạn không có quyền xem dashboard nhân viên.";
        return result;
    }
    const json response = m_database.execute(
        "SELECT "
        "(SELECT COUNT(*) FROM users), "
        "(SELECT COUNT(*) FROM users WHERE role = 'CUSTOMER'), "
        "(SELECT COUNT(*) FROM transactions), "
        "(SELECT COALESCE(SUM(balance_vnd), 0) FROM accounts), "
        "(SELECT COUNT(*) FROM loans WHERE status = 'PENDING'), "
        "(SELECT COUNT(*) FROM users WHERE status = 'BLOCKED'), "
        "(SELECT COUNT(*) FROM transactions WHERE date(created_at) = date('now')),"
        "(SELECT COUNT(DISTINCT a.user_id) FROM transactions t JOIN accounts a ON a.account_id = t.receiver_account_id "
        " WHERE t.type = 'DEPOSIT_COUNTER' AND date(t.created_at) = date('now'));"
    );
    if (hasDatabaseError(response)) { result.message = "Không tải được thống kê dashboard từ database."; return result; }
    try {
        const auto rows = response["results"][0]["response"]["result"]["rows"];
        if (!rows.is_array() || rows.empty() || !rows[0].is_array() || rows[0].size() < 8)
            throw std::runtime_error("bad dashboard result");
        result.total_users = std::stoll(cellText(rows[0][0]));
        result.total_customers = std::stoll(cellText(rows[0][1]));
        result.total_transactions = std::stoll(cellText(rows[0][2]));
        result.total_balance_vnd = std::stoll(cellText(rows[0][3]));
        result.pending_loans = std::stoll(cellText(rows[0][4]));
        result.locked_users = std::stoll(cellText(rows[0][5]));
        result.today_transactions = std::stoll(cellText(rows[0][6]));
        result.today_served_customers = std::stoll(cellText(rows[0][7]));
    } catch (...) { result.message = "Dữ liệu dashboard không hợp lệ."; return result; }
    result.success = true;
    result.message = "Đã tải thống kê dashboard.";
    return result;
}

StaffOperationResult StaffService::createStaff(const CreateStaffRequest& request) {
    StaffOperationResult result;
    std::string operatorId;
    if (!hasAdminPermission(request.session_token, Permission::ADMIN_MANAGE_STAFF, &operatorId)) {
        result.message = "BẢO MẬT: Bạn không có quyền tạo tài khoản nhân viên.";
        return result;
    }

    const bool hasUpper = std::any_of(request.password.begin(), request.password.end(), [](unsigned char c) { return std::isupper(c) != 0; });
    const bool hasLower = std::any_of(request.password.begin(), request.password.end(), [](unsigned char c) { return std::islower(c) != 0; });
    const bool hasDigit = std::any_of(request.password.begin(), request.password.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    if (request.username.size() < 4 || request.username.size() > 32 || !hasOnlyUsernameCharacters(request.username) ||
        request.fullname.size() < 2 || request.fullname.size() > 100 || !validEmail(request.email) ||
        request.phone.size() != 10 || !allDigits(request.phone) ||
        request.cccd.size() != 12 || !allDigits(request.cccd) ||
        (request.role != "TELLER" && request.role != "ADMIN") ||
        request.password.size() < 12 || request.password.size() > 128 || !hasUpper || !hasLower || !hasDigit) {
        result.message = "Thông tin không hợp lệ. Tên đăng nhập 4–32 ký tự; SĐT 10 số; CCCD 12 số; mật khẩu ít nhất 12 ký tự gồm chữ hoa, chữ thường và số.";
        return result;
    }

    const std::string salt = AuthSecurity::generateSalt();
    const std::string passwordHash = AuthSecurity::hashPasswordPBKDF2(request.password, salt);
    const std::string staffId = "STF_" + AuthSecurity::generateNumericCode(24);
    if (!m_userRepository.createStaff(staffId, request.username, passwordHash, salt,
                                      request.fullname, request.email, request.phone,
                                      request.cccd, request.role)) {
        result.message = "Không tạo được nhân viên. Tên đăng nhập, email, số điện thoại hoặc CCCD có thể đã được sử dụng.";
        return result;
    }

    const bool auditWritten = m_auditRepository.logAction(
        operatorId, "CREATE_STAFF", staffId, "Created staff account with role " + request.role);
    result.success = true;
    result.created_id = staffId;
    result.message = auditWritten ? "Đã tạo tài khoản nhân viên." : "Đã tạo tài khoản nhưng không ghi được audit log.";
    return result;
}

SystemConfigDTO StaffService::getSystemConfig(const GetSystemConfigRequest& request) {
    SystemConfigDTO result;
    const auto session = m_sessionManager.getSession(request.session_token);
    const auto user = session ? m_userRepository.findById(session->user_id) : nullptr;
    const bool isAdminReader = session && session->role == "ADMIN" && user && user->isActive() &&
                               user->hasPermission(Permission::ADMIN_SYSTEM_CONFIG);
    const bool isTellerReader = session && session->role == "TELLER" && user && user->isActive() &&
                                user->hasPermission(Permission::TELLER_COUNTER_CASH);
    if (!isAdminReader && !isTellerReader) {
        result.message = "BẢO MẬT: Bạn không có quyền xem cấu hình hệ thống.";
        return result;
    }

    const json response = m_database.execute("SELECT key, value FROM system_config;");
    if (hasDatabaseError(response)) {
        result.message = "Không đọc được cấu hình từ bảng system_config.";
        return result;
    }

    std::map<std::string, std::string> values;
    try {
        const auto rows = response["results"][0]["response"]["result"]["rows"];
        if (rows.is_array()) {
            for (const auto& row : rows) {
                if (row.size() >= 2) values[cellText(row[0])] = cellText(row[1]);
            }
        }
    } catch (...) {
        result.message = "Cấu trúc dữ liệu cấu hình không hợp lệ.";
        return result;
    }

    try {
        if (values.count("admin.interbank_fee_vnd") != 0)
            result.interbank_fee_vnd = std::stoll(values.at("admin.interbank_fee_vnd"));
        if (values.count("admin.atm_fee_vnd") != 0)
            result.atm_fee_vnd = std::stoll(values.at("admin.atm_fee_vnd"));
        if (values.count("admin.savings_max_rate_percent") != 0)
            result.savings_max_rate_percent = std::stod(values.at("admin.savings_max_rate_percent"));
        if (values.count("credit.monthly_rate_percent") != 0)
            result.credit_monthly_rate_percent = std::stod(values.at("credit.monthly_rate_percent"));
        if (values.count("credit.due_day") != 0)
            result.credit_due_day = std::stoi(values.at("credit.due_day"));
        if (values.count("credit.minimum_payment_percent") != 0)
            result.credit_min_payment_percent = std::stod(values.at("credit.minimum_payment_percent"));
        if (values.count("counter.max_transaction_vnd") != 0)
            result.counter_max_transaction_vnd = std::stoll(values.at("counter.max_transaction_vnd"));
        result.loan_terms = defaultLoanTerms();
        if (values.count("loan.terms") != 0) {
            const json terms = json::parse(values.at("loan.terms"));
            result.loan_terms = terms.get<std::vector<LoanTermConfigDTO>>();
        }
    } catch (...) {
        result.message = "Giá trị cấu hình trong database không hợp lệ.";
        return result;
    }

    if (result.loan_terms.empty()) result.loan_terms = defaultLoanTerms();
    result.success = true;
    result.message = "Đã tải cấu hình hệ thống.";
    return result;
}

StaffOperationResult StaffService::updateSystemConfig(const UpdateSystemConfigRequest& request) {
    StaffOperationResult result;
    std::string operatorId;
    if (!isAdmin(request.session_token, &operatorId)) {
        result.message = "BẢO MẬT: Chỉ quản trị viên đang hoạt động mới được sửa cấu hình hệ thống.";
        return result;
    }

    if (request.interbank_fee_vnd < 0 || request.interbank_fee_vnd > 1000000000LL ||
        request.atm_fee_vnd < 0 || request.atm_fee_vnd > 1000000000LL) {
        result.message = "Phí giao dịch phải nằm trong khoảng 0 đến 1.000.000.000 đồng.";
        return result;
    }
    if (request.savings_max_rate_percent < 0.0 || request.savings_max_rate_percent > 50.0) {
        result.message = "Lãi suất tiết kiệm phải nằm trong khoảng 0 đến 50%/năm.";
        return result;
    }
    if (request.credit_monthly_rate_percent < 0.0 || request.credit_monthly_rate_percent > 20.0 ||
        request.credit_due_day < 1 || request.credit_due_day > 28 ||
        request.credit_min_payment_percent < 1.0 || request.credit_min_payment_percent > 100.0) {
        result.message = "Lãi suất tín dụng phải từ 0 đến 20%/tháng, ngày đến hạn từ 1 đến 28 và tỷ lệ trả tối thiểu từ 1 đến 100%.";
        return result;
    }
    if (request.counter_max_transaction_vnd < 1000000 || request.counter_max_transaction_vnd > 1000000000LL) {
        result.message = "Hạn mức mỗi giao dịch tại quầy phải từ 1.000.000 đến 1.000.000.000 đồng.";
        return result;
    }

    std::set<int> monthsSeen;
    bool hasEnabledTerm = false;
    for (const auto& term : request.loan_terms) {
        if (term.months < 1 || term.months > 360 || !monthsSeen.insert(term.months).second ||
            term.rate < 0.0 || term.rate > 50.0) {
            result.message = "Kỳ hạn vay phải duy nhất, từ 1 đến 360 tháng; lãi suất từ 0 đến 50%/năm.";
            return result;
        }
        hasEnabledTerm = hasEnabledTerm || term.enabled;
    }
    if (request.loan_terms.empty() || !hasEnabledTerm) {
        result.message = "Cần giữ ít nhất một kỳ hạn vay đang bật.";
        return result;
    }

    const json terms = request.loan_terms;
    const std::string termsText = terms.dump();
    const std::vector<std::pair<std::string, json>> statements = {
        {"INSERT INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) "
         "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP;",
         json::array({{{"type", "text"}, {"value", "admin.interbank_fee_vnd"}},
                      {{"type", "text"}, {"value", std::to_string(request.interbank_fee_vnd)}}})},
        {"INSERT INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) "
         "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP;",
         json::array({{{"type", "text"}, {"value", "admin.atm_fee_vnd"}},
                      {{"type", "text"}, {"value", std::to_string(request.atm_fee_vnd)}}})},
        {"INSERT INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) "
         "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP;",
         json::array({{{"type", "text"}, {"value", "admin.savings_max_rate_percent"}},
                      {{"type", "text"}, {"value", std::to_string(request.savings_max_rate_percent)}}})},
        {"INSERT INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) "
         "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP;",
         json::array({{{"type", "text"}, {"value", "loan.terms"}},
                      {{"type", "text"}, {"value", termsText}}})},
        {"INSERT INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) ON CONFLICT(key) DO UPDATE SET value=excluded.value,updated_at=CURRENT_TIMESTAMP;",
         json::array({{{"type","text"},{"value","credit.monthly_rate_percent"}},{{"type","text"},{"value",std::to_string(request.credit_monthly_rate_percent)}}})},
        {"INSERT INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) ON CONFLICT(key) DO UPDATE SET value=excluded.value,updated_at=CURRENT_TIMESTAMP;",
         json::array({{{"type","text"},{"value","credit.due_day"}},{{"type","text"},{"value",std::to_string(request.credit_due_day)}}})},
        {"INSERT INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) ON CONFLICT(key) DO UPDATE SET value=excluded.value,updated_at=CURRENT_TIMESTAMP;",
         json::array({{{"type","text"},{"value","credit.minimum_payment_percent"}},{{"type","text"},{"value",std::to_string(request.credit_min_payment_percent)}}})},
        {"INSERT INTO system_config (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) ON CONFLICT(key) DO UPDATE SET value=excluded.value,updated_at=CURRENT_TIMESTAMP;",
         json::array({{{"type","text"},{"value","counter.max_transaction_vnd"}},{{"type","text"},{"value",std::to_string(request.counter_max_transaction_vnd)}}})}
    };

    const json response = m_database.executeBatch(statements);
    if (hasDatabaseError(response)) {
        result.message = "Không lưu được cấu hình xuống database.";
        return result;
    }

    const bool auditWritten = m_auditRepository.logAction(
        operatorId, "UPDATE_SYSTEM_CONFIG", "system_config", "Updated fees, savings rate and loan terms");
    result.success = true;
    result.message = auditWritten
        ? "Đã lưu cấu hình hệ thống."
        : "Cấu hình đã lưu nhưng không ghi được audit log.";
    return result;
}

} // namespace Banking
