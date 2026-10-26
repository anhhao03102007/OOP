#include "services/CreditService.hpp"
#include "core/AuthSecurity.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <utility>
#include <vector>

namespace Banking {
namespace {
using Params = json;
Params text(const std::string& value) { return Params{{"type", "text"}, {"value", value}}; }
Params number(int64_t value) { return Params{{"type", "numeric"}, {"value", value}}; }
std::string cellText(const json& cell) {
    if (!cell.is_object() || !cell.contains("value") || cell["value"].is_null()) return {};
    const auto& value = cell["value"];
    return value.is_string() ? value.get<std::string>() : value.dump();
}
bool hasError(const json& response) {
    if (!response.contains("results") || !response["results"].is_array() || response["results"].empty()) return true;
    for (const auto& item : response["results"]) if (item.value("type", "") == "error") return true;
    return false;
}
bool validDocument(const std::string& fileName, const std::string& mimeType,
                   int64_t fileSize, const std::string& storageKey) {
    const bool mimeAllowed = mimeType == "application/pdf" || mimeType == "image/jpeg" || mimeType == "image/png";
    const std::string cloudinaryPrefix = "https://res.cloudinary.com/ltcqqgmq/";
    return !fileName.empty() && fileName.size() <= 255 && fileName.find_first_of("\r\n") == std::string::npos &&
        mimeAllowed && fileSize > 0 && fileSize <= 5 * 1024 * 1024 &&
        storageKey.rfind(cloudinaryPrefix, 0) == 0;
}
json rowsOf(const json& response) {
    try { return response["results"][0]["response"]["result"]["rows"]; } catch (...) { return json::array(); }
}
}

CreditService::CreditService(TursoDB& database, UserRepository& userRepository, SessionManager& sessionManager)
    : db(database), users(userRepository), sessions(sessionManager) {}

bool CreditService::ensureSchema() const {
    const std::vector<std::pair<std::string, json>> ddl = {
        {"CREATE TABLE IF NOT EXISTS credit_applications (application_id TEXT PRIMARY KEY, user_id TEXT NOT NULL, requested_limit_vnd INTEGER NOT NULL, monthly_income_vnd INTEGER NOT NULL DEFAULT 0, status TEXT NOT NULL DEFAULT 'PENDING', decided_by TEXT NOT NULL DEFAULT '', decision_note TEXT NOT NULL DEFAULT '', account_id TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, decided_at TEXT, FOREIGN KEY(user_id) REFERENCES users(user_id));", json::array()},
        {"CREATE INDEX IF NOT EXISTS idx_credit_applications_user ON credit_applications(user_id, created_at DESC);", json::array()},
        {"CREATE UNIQUE INDEX IF NOT EXISTS idx_credit_applications_one_pending ON credit_applications(user_id) WHERE status='PENDING';", json::array()},
        {"CREATE TABLE IF NOT EXISTS credit_lines (account_id TEXT PRIMARY KEY, credit_limit_vnd INTEGER NOT NULL, monthly_rate_percent REAL NOT NULL, due_day INTEGER NOT NULL, minimum_payment_percent REAL NOT NULL, opened_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, last_interest_date TEXT NOT NULL DEFAULT '1970-01-01', FOREIGN KEY(account_id) REFERENCES accounts(account_id));", json::array()},
        {"CREATE TABLE IF NOT EXISTS credit_application_documents (document_id TEXT PRIMARY KEY, application_id TEXT NOT NULL UNIQUE, user_id TEXT NOT NULL, file_name TEXT NOT NULL, mime_type TEXT NOT NULL, file_size INTEGER NOT NULL CHECK(file_size>0 AND file_size<=5242880), storage_key TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'PENDING_REVIEW' CHECK(status IN ('PENDING_REVIEW','APPROVED','REJECTED')), review_note TEXT NOT NULL DEFAULT '', reviewed_by TEXT, submitted_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, FOREIGN KEY(application_id) REFERENCES credit_applications(application_id) ON DELETE CASCADE, FOREIGN KEY(user_id) REFERENCES users(user_id));", json::array()},
        {"CREATE INDEX IF NOT EXISTS idx_credit_application_documents_status ON credit_application_documents(status, submitted_at DESC);", json::array()}
    };
    if(hasError(db.executeBatch(ddl))) return false;
    const json applicationColumns=db.execute("PRAGMA table_info(credit_applications);");
    if(hasError(applicationColumns)) return false;
    bool hasIncome=false;
    for(const auto& row:rowsOf(applicationColumns)) if(row.size()>1&&cellText(row[1])=="monthly_income_vnd") hasIncome=true;
    if(!hasIncome && hasError(db.execute("ALTER TABLE credit_applications ADD COLUMN monthly_income_vnd INTEGER NOT NULL DEFAULT 0;"))) return false;
    const json columns=db.execute("PRAGMA table_info(credit_lines);");
    if(hasError(columns)) return false;
    bool hasInterestDate=false;
    for(const auto& row:rowsOf(columns)) if(row.size()>1&&cellText(row[1])=="last_interest_date") hasInterestDate=true;
    if(!hasInterestDate && hasError(db.execute("ALTER TABLE credit_lines ADD COLUMN last_interest_date TEXT NOT NULL DEFAULT '1970-01-01';"))) return false;
    return true;
}

bool CreditService::accrueInterest(const std::string& accountId, const std::string& userId) const {
    for(int month=0; month<60; ++month) {
        const json balanceResult=db.execute("SELECT a.balance_vnd,c.monthly_rate_percent FROM accounts a JOIN credit_lines c ON c.account_id=a.account_id WHERE a.account_id=? AND a.user_id=? AND a.account_type='CREDIT' AND a.status='ACTIVE' LIMIT 1;",json::array({text(accountId),text(userId)}));
        if(hasError(balanceResult)) return false;
        const auto balanceRows=rowsOf(balanceResult);
        if(balanceRows.empty()) return true;
        int64_t interest=0;
        try { interest=static_cast<int64_t>(std::llround(std::stoll(cellText(balanceRows[0][0]))*std::stod(cellText(balanceRows[0][1]))/100.0)); }
        catch (...) { return false; }
        const std::string interestTx="TXN_"+AuthSecurity::generateNumericCode(24);
        const json result=db.executeBatch({
            {"UPDATE credit_lines SET last_interest_date=date(last_interest_date,'+1 month') WHERE account_id=? AND date(last_interest_date,'+1 month')<=date('now') AND EXISTS(SELECT 1 FROM accounts WHERE account_id=? AND user_id=? AND account_type='CREDIT' AND status='ACTIVE');",json::array({text(accountId),text(accountId),text(userId)})},
            {"UPDATE accounts SET balance_vnd=balance_vnd+? WHERE account_id=? AND user_id=? AND account_type='CREDIT' AND status='ACTIVE' AND changes()=1;",json::array({number(interest),text(accountId),text(userId)})},
            {"INSERT INTO transactions(transaction_id,sender_account_id,receiver_account_id,amount_vnd,fee_vnd,type,status,description,post_balance_vnd,created_at) SELECT ?,'BANK',?,?,0,'CREDIT_INTEREST','SUCCESS','Lãi tín dụng hàng tháng',(SELECT balance_vnd FROM accounts WHERE account_id=?),CURRENT_TIMESTAMP WHERE changes()=1 AND ?>0;",json::array({text(interestTx),text(accountId),number(interest),text(accountId),number(interest)})}
        });
        if(hasError(result)) return false;
        const json due=db.execute("SELECT date(last_interest_date,'+1 month')<=date('now') FROM credit_lines WHERE account_id=?;",json::array({text(accountId)}));
        const auto rows=rowsOf(due);
        if(rows.empty()||cellText(rows[0][0])!="1") return true;
    }
    return true;
}

CreditAccountDTO CreditService::getMyAccount(const std::string& token) const {
    CreditAccountDTO result;
    const auto session = sessions.getSession(token);
    if (!session || session->role != "CUSTOMER") { result.message = "Phiên khách hàng không hợp lệ."; return result; }
    if (!ensureSchema()) { result.message = "Không truy cập được dữ liệu hạn mức tín dụng."; return result; }
    const json accountIdResponse=db.execute("SELECT account_id FROM accounts WHERE user_id=? AND account_type='CREDIT' AND status='ACTIVE' ORDER BY created_at DESC LIMIT 1;",json::array({text(session->user_id)}));
    if(!hasError(accountIdResponse)&&!rowsOf(accountIdResponse).empty()) {
        const auto id=cellText(rowsOf(accountIdResponse)[0][0]);
        if(!accrueInterest(id,session->user_id)){result.message="Không cập nhật được lãi tín dụng.";return result;}
    }
    const json response = db.execute(
        "SELECT a.account_id,a.balance_vnd,c.credit_limit_vnd,c.monthly_rate_percent,c.due_day,c.minimum_payment_percent "
        "FROM accounts a JOIN credit_lines c ON c.account_id=a.account_id WHERE a.user_id=? AND a.account_type='CREDIT' AND a.status='ACTIVE' ORDER BY a.created_at DESC LIMIT 1;",
        json::array({text(session->user_id)}));
    if (hasError(response)) { result.message = "Không đọc được tài khoản tín dụng."; return result; }
    const auto rows = rowsOf(response);
    if (!rows.is_array() || rows.empty()) { result.message = "Chưa có hạn mức tín dụng được Admin phê duyệt."; return result; }
    try {
        const auto& row = rows[0];
        result.account_id = cellText(row[0]); result.used_vnd = std::stoll(cellText(row[1]));
        result.limit_vnd = std::stoll(cellText(row[2])); result.monthly_rate_percent = std::stod(cellText(row[3]));
        result.due_day = std::stoi(cellText(row[4])); result.minimum_payment_percent = std::stod(cellText(row[5]));
        result.available_vnd = std::max<int64_t>(0, result.limit_vnd - result.used_vnd);
        result.minimum_payment_vnd = result.used_vnd == 0 ? 0 : std::max<int64_t>(1000, static_cast<int64_t>(result.used_vnd * result.minimum_payment_percent / 100.0));
        const json checking = db.execute("SELECT account_id,balance_vnd FROM accounts WHERE user_id=? AND account_type='CHECKING' AND status='ACTIVE' ORDER BY created_at,account_id;",
            json::array({text(session->user_id)}));
        if (hasError(checking)) { result.message = "Không tải được tài khoản thanh toán để trả nợ."; return result; }
        for (const auto& checkingRow : rowsOf(checking)) {
            result.checking_accounts.push_back({cellText(checkingRow[0]), std::stoll(cellText(checkingRow[1]))});
        }
        result.success = true; result.message = "Đã tải hạn mức tín dụng.";
    } catch (...) { result.message = "Dữ liệu hạn mức tín dụng không hợp lệ."; }
    return result;
}

CreditApplicationsResult CreditService::getApplications(const CreditApplicationsRequest& request) const {
    CreditApplicationsResult result;
    const auto session = sessions.getSession(request.session_token);
    if (!session || !users.findById(session->user_id)) { result.message = "Phiên đăng nhập không hợp lệ."; return result; }
    if (session->role != "CUSTOMER" && session->role != "ADMIN" && session->role != "TELLER") { result.message = "Không đủ quyền xem hồ sơ tín dụng."; return result; }
    if (!ensureSchema()) { result.message = "Không truy cập được hồ sơ tín dụng."; return result; }
    const std::string sql = session->role == "CUSTOMER"
        ? "SELECT a.application_id,a.user_id,a.requested_limit_vnd,a.monthly_income_vnd,a.status,a.decision_note,a.account_id,a.created_at,d.document_id,d.file_name,d.mime_type,d.file_size,d.storage_key,d.status,d.review_note FROM credit_applications a LEFT JOIN credit_application_documents d ON d.application_id=a.application_id WHERE a.user_id=? ORDER BY a.created_at DESC;"
        : "SELECT a.application_id,a.user_id,a.requested_limit_vnd,a.monthly_income_vnd,a.status,a.decision_note,a.account_id,a.created_at,d.document_id,d.file_name,d.mime_type,d.file_size,d.storage_key,d.status,d.review_note FROM credit_applications a LEFT JOIN credit_application_documents d ON d.application_id=a.application_id ORDER BY a.created_at DESC;";
    const json response = db.execute(sql, session->role == "CUSTOMER" ? json::array({text(session->user_id)}) : json::array());
    if (hasError(response)) { result.message = "Không tải được danh sách hồ sơ."; return result; }
    for (const auto& row : rowsOf(response)) {
        try { const std::string fileSize = cellText(row[11]); result.applications.push_back({cellText(row[0]), cellText(row[1]), std::stoll(cellText(row[2])),
            std::stoll(cellText(row[3])), cellText(row[4]), cellText(row[5]), cellText(row[6]), cellText(row[7]),
            cellText(row[8]), cellText(row[9]), cellText(row[10]), fileSize.empty() ? 0 : std::stoll(fileSize),
            cellText(row[12]), cellText(row[13]), cellText(row[14])}); }
        catch (...) { result.message = "Có hồ sơ tín dụng sai định dạng."; return result; }
    }
    result.success = true; result.message = "Đã tải hồ sơ tín dụng.";
    return result;
}

CreditApplicationsResult CreditService::apply(const CreditApplicationRequest& request) const {
    CreditApplicationsResult result;
    const auto session = sessions.getSession(request.session_token);
    if (!session) { result.message = "Phiên đăng nhập không hợp lệ."; return result; }
    const bool staffSession = session->role == "TELLER" || session->role == "ADMIN";
    const std::string targetUserId = request.target_user_id.empty() ? session->user_id : request.target_user_id;
    if (session->role != "CUSTOMER" && !staffSession) { result.message = "Bạn không có quyền nộp hồ sơ tín dụng."; return result; }
    if (session->role == "CUSTOMER" && targetUserId != session->user_id) { result.message = "Khách hàng chỉ được xin hạn mức cho chính mình."; return result; }
    if (staffSession && request.target_user_id.empty()) { result.message = "Thiếu thông tin khách hàng cần xin hạn mức."; return result; }
    if (staffSession && !request.kyc_verified) { result.message = "Cần xác nhận đã đối chiếu CCCD bản gốc trước khi nộp hồ sơ."; return result; }
    const auto user = users.findById(targetUserId);
    if (!user || user->getRole() != "CUSTOMER" || !user->isActive()) { result.message = "Khách hàng không tồn tại hoặc không hoạt động."; return result; }
    if (staffSession && !user->verifyPin(request.customer_pin)) { result.message = "Mã PIN giao dịch của khách hàng không chính xác."; return result; }
    if (request.monthly_income_vnd < 1000000 || request.monthly_income_vnd > 10000000000LL) { result.message = "Thu nhập tháng khai báo phải từ 1.000.000 đến 10.000.000.000 đồng."; return result; }
    const int64_t eligibleLimit = std::min<int64_t>(500000000, request.monthly_income_vnd * 6);
    if (request.requested_limit_vnd < 1000000 || request.requested_limit_vnd > eligibleLimit) { result.message = "Hạn mức đề nghị phải từ 1.000.000 đồng và không vượt quá 6 lần thu nhập tháng (tối đa 500.000.000 đồng)."; return result; }
    if (!validDocument(request.file_name, request.mime_type, request.file_size, request.storage_key)) {
        result.message = "Hồ sơ cần có giấy tờ chứng minh thu nhập dạng PDF/JPG/PNG, tối đa 5 MB."; return result;
    }
    if (!ensureSchema()) { result.message = "Không truy cập được hồ sơ tín dụng."; return result; }
    const json pending = db.execute("SELECT application_id FROM credit_applications WHERE user_id=? AND status='PENDING' LIMIT 1;", json::array({text(targetUserId)}));
    if (hasError(pending)) { result.message = "Không kiểm tra được hồ sơ đang chờ."; return result; }
    if (!rowsOf(pending).empty()) { result.message = "Bạn đã có hồ sơ tín dụng đang chờ Admin duyệt."; return result; }
    const json active = db.execute("SELECT account_id FROM accounts WHERE user_id=? AND account_type='CREDIT' AND status='ACTIVE' LIMIT 1;", json::array({text(targetUserId)}));
    if (hasError(active)) { result.message = "Không kiểm tra được hạn mức hiện có."; return result; }
    if (!rowsOf(active).empty()) { result.message = "Bạn đã có hạn mức tín dụng đang hoạt động."; return result; }
    const std::string id = "CRAPP_" + AuthSecurity::generateNumericCode(24);
    const std::string documentId = "CRDOC_" + AuthSecurity::generateNumericCode(24);
    std::vector<std::pair<std::string, json>> statements = {
        {"INSERT INTO credit_applications(application_id,user_id,requested_limit_vnd,monthly_income_vnd,status) VALUES(?,?,?,?,'PENDING');",
            json::array({text(id), text(targetUserId), number(request.requested_limit_vnd), number(request.monthly_income_vnd)})},
        {"INSERT INTO credit_application_documents(document_id,application_id,user_id,file_name,mime_type,file_size,storage_key,status) SELECT ?,?,?,?,?,?,?,'PENDING_REVIEW' WHERE changes()=1;",
            json::array({text(documentId), text(id), text(targetUserId), text(request.file_name), text(request.mime_type), number(request.file_size), text(request.storage_key)})}
    };
    if (staffSession) {
        const std::string auditId = "CRLOG_" + AuthSecurity::generateNumericCode(20);
        const std::string notificationId = "NTF_" + AuthSecurity::generateNumericCode(20);
        statements.push_back({
            "INSERT INTO audit_logs(log_id,operator_id,action,target_id,details,created_at) SELECT ?,?,?,?, ?,CURRENT_TIMESTAMP WHERE changes()=1;",
            json::array({text(auditId), text(session->user_id), text("CREDIT_APPLICATION_SUBMITTED"),
                text(id), text("Customer: " + targetUserId + ", requested limit: " +
                    std::to_string(request.requested_limit_vnd) + " VND")})
        });
        statements.push_back({
            "INSERT INTO notifications(notification_id,user_id,title,body,notification_type,reference_id,created_at) SELECT ?,?,?,?,?,?,CURRENT_TIMESTAMP WHERE changes()=1;",
            json::array({text(notificationId), text(targetUserId), text("Đã tiếp nhận hồ sơ tín dụng"),
                text("Hồ sơ " + id + " đã được ghi nhận và đang chờ Admin kiểm tra."),
                text("CREDIT_APPLICATION"), text(id)})
        });
    }
    const json saved = db.executeBatch(statements);
    if (hasError(saved)) { result.message = "Không lưu được hồ sơ xin hạn mức."; return result; }
    const auto verify = db.execute("SELECT document_id FROM credit_application_documents WHERE application_id=? LIMIT 1;", json::array({text(id)}));
    if (hasError(verify) || rowsOf(verify).empty()) { result.message = "Database chưa xác nhận giấy tờ chứng minh thu nhập."; return result; }
    CreditApplicationsResult created = getApplications({request.session_token});
    if (created.success) created.created_application_id = id;
    return created;
}

CreditDocumentResult CreditService::submitDocument(const CreditDocumentRequest& request) const {
    CreditDocumentResult result;
    const auto session = sessions.getSession(request.session_token);
    const auto user = session ? users.findById(session->user_id) : nullptr;
    if (!session || session->role != "CUSTOMER" || !user || !user->isActive()) {
        result.message = "Chỉ khách hàng đang hoạt động mới được nộp lại giấy tờ."; return result;
    }
    if (request.application_id.empty() || !validDocument(request.file_name, request.mime_type, request.file_size, request.storage_key)) {
        result.message = "Giấy tờ phải là PDF/JPG/PNG và tối đa 5 MB."; return result;
    }
    if (!ensureSchema()) { result.message = "Không truy cập được hồ sơ tín dụng."; return result; }
    const json existing = db.execute("SELECT document_id,status FROM credit_application_documents WHERE application_id=? AND user_id=? LIMIT 1;",
        json::array({text(request.application_id), text(session->user_id)}));
    if (hasError(existing)) { result.message = "Không kiểm tra được giấy tờ hiện tại."; return result; }
    json saved;
    if (rowsOf(existing).empty()) {
        const std::string documentId = "CRDOC_" + AuthSecurity::generateNumericCode(24);
        saved = db.execute("INSERT INTO credit_application_documents(document_id,application_id,user_id,file_name,mime_type,file_size,storage_key,status) "
            "SELECT ?,?,?,?,?,?,?,'PENDING_REVIEW' WHERE EXISTS(SELECT 1 FROM credit_applications WHERE application_id=? AND user_id=? AND status='PENDING');",
            json::array({text(documentId), text(request.application_id), text(session->user_id), text(request.file_name), text(request.mime_type),
                number(request.file_size), text(request.storage_key), text(request.application_id), text(session->user_id)}));
    } else if (cellText(rowsOf(existing)[0][1]) == "REJECTED") {
        saved = db.execute(
            "UPDATE credit_application_documents SET file_name=?,mime_type=?,file_size=?,storage_key=?,status='PENDING_REVIEW',review_note='',reviewed_by=NULL,submitted_at=CURRENT_TIMESTAMP,updated_at=CURRENT_TIMESTAMP "
            "WHERE application_id=? AND user_id=? AND status='REJECTED' AND EXISTS(SELECT 1 FROM credit_applications WHERE application_id=? AND user_id=? AND status='PENDING');",
            json::array({text(request.file_name), text(request.mime_type), number(request.file_size), text(request.storage_key),
                text(request.application_id), text(session->user_id), text(request.application_id), text(session->user_id)}));
    } else {
        result.message = "Chỉ có thể nộp giấy tờ khi hồ sơ đang yêu cầu bổ sung."; return result;
    }
    if (hasError(saved)) { result.message = "Không lưu được giấy tờ bổ sung."; return result; }
    const json verify = db.execute("SELECT document_id FROM credit_application_documents WHERE application_id=? AND user_id=? AND status='PENDING_REVIEW' AND storage_key=? LIMIT 1;",
        json::array({text(request.application_id), text(session->user_id), text(request.storage_key)}));
    if (hasError(verify) || rowsOf(verify).empty()) { result.message = "Hồ sơ không yêu cầu nộp lại giấy tờ hoặc đã được xử lý."; return result; }
    result.success = true;
    result.message = "Đã gửi lại giấy tờ để Admin kiểm tra.";
    return result;
}

CreditDocumentResult CreditService::reviewDocument(const ReviewCreditDocumentRequest& request) const {
    CreditDocumentResult result;
    const auto session = sessions.getSession(request.session_token);
    const auto operatorUser = session ? users.findById(session->user_id) : nullptr;
    if (!session || (session->role != "ADMIN" && session->role != "TELLER") || !operatorUser || !operatorUser->isActive()) {
        result.message = "Chỉ nhân viên đang hoạt động mới được kiểm tra giấy tờ tín dụng."; return result;
    }
    const bool approve = request.action == "APPROVE";
    if ((!approve && request.action != "REQUEST_RESUBMISSION") || request.document_id.empty() ||
        (!approve && (request.note.empty() || request.note.size() > 500))) {
        result.message = "Hãy chọn xác nhận hoặc yêu cầu nộp lại và ghi rõ lý do."; return result;
    }
    if (!ensureSchema()) { result.message = "Không truy cập được giấy tờ tín dụng."; return result; }
    const auto document = db.execute("SELECT application_id,user_id FROM credit_application_documents WHERE document_id=? AND status='PENDING_REVIEW' LIMIT 1;",
        json::array({text(request.document_id)}));
    const auto documentRows = rowsOf(document);
    if (hasError(document) || documentRows.empty()) { result.message = "Giấy tờ không tồn tại hoặc đã được xử lý."; return result; }
    const std::string applicationId = cellText(documentRows[0][0]);
    const std::string customerId = cellText(documentRows[0][1]);
    const std::string status = approve ? "APPROVED" : "REJECTED";
    const std::string title = approve ? "Đã xác nhận giấy tờ tín dụng" : "Cần bổ sung giấy tờ tín dụng";
    const std::string body = approve ? "Giấy tờ chứng minh thu nhập đã được xác nhận." : "Vui lòng nộp lại giấy tờ chứng minh thu nhập. Lý do: " + request.note;
    const std::string auditId = "CRLOG_" + AuthSecurity::generateNumericCode(20);
    const std::string notificationId = "NTF_" + AuthSecurity::generateNumericCode(20);
    const auto saved = db.executeBatch({
        {"UPDATE credit_application_documents SET status=?,review_note=?,reviewed_by=?,updated_at=CURRENT_TIMESTAMP WHERE document_id=? AND status='PENDING_REVIEW' AND EXISTS(SELECT 1 FROM credit_applications WHERE application_id=? AND status='PENDING');",
            json::array({text(status), text(approve ? std::string{} : request.note), text(session->user_id), text(request.document_id), text(applicationId)})},
        {"INSERT INTO audit_logs(log_id,operator_id,action,target_id,details,created_at) SELECT ?,?,?,?, ?,CURRENT_TIMESTAMP WHERE changes()=1;",
            json::array({text(auditId), text(session->user_id), text(approve ? "CREDIT_DOCUMENT_APPROVED" : "CREDIT_DOCUMENT_RESUBMISSION_REQUESTED"), text(applicationId), text(approve ? "Income document approved" : request.note)})},
        {"INSERT INTO notifications(notification_id,user_id,title,body,notification_type,reference_id,created_at) SELECT ?,?,?,?,?,?,CURRENT_TIMESTAMP WHERE changes()=1;",
            json::array({text(notificationId), text(customerId), text(title), text(body), text("CREDIT_DOCUMENT"), text(applicationId)})}
    });
    if (hasError(saved)) { result.message = "Không lưu được kết quả kiểm tra giấy tờ."; return result; }
    const auto verify = db.execute("SELECT status FROM credit_application_documents WHERE document_id=?;", json::array({text(request.document_id)}));
    if (hasError(verify) || rowsOf(verify).empty() || cellText(rowsOf(verify)[0][0]) != status) {
        result.message = "Database chưa xác nhận trạng thái giấy tờ."; return result;
    }
    result.success = true;
    result.message = approve ? "Đã xác nhận giấy tờ thu nhập." : "Đã yêu cầu khách hàng nộp lại giấy tờ.";
    return result;
}

CreditApplicationsResult CreditService::decide(const CreditDecisionRequest& request) const {
    CreditApplicationsResult result;
    const auto session = sessions.getSession(request.session_token);
    const auto admin = session ? users.findById(session->user_id) : nullptr;
    if (!session || session->role != "ADMIN" || !admin || !admin->isActive()) { result.message = "Chỉ Admin đang hoạt động mới được duyệt hạn mức."; return result; }
    if ((request.action != "APPROVE" && request.action != "REJECT") || request.application_id.empty()) { result.message = "Quyết định tín dụng không hợp lệ."; return result; }
    if (request.action == "APPROVE" && (request.approved_limit_vnd < 1000000 || request.approved_limit_vnd > 500000000)) { result.message = "Hạn mức duyệt phải từ 1.000.000 đến 500.000.000 đồng."; return result; }
    if (!ensureSchema()) { result.message = "Không truy cập được hồ sơ tín dụng."; return result; }
    const json termsResponse = db.execute("SELECT key,value FROM system_config WHERE key IN ('credit.monthly_rate_percent','credit.due_day','credit.minimum_payment_percent');");
    if (hasError(termsResponse)) { result.message = "Không đọc được điều kiện tín dụng do Admin cấu hình."; return result; }
    double rate = 2.0, minimum = 5.0; int dueDay = 15;
    for (const auto& row : rowsOf(termsResponse)) {
        try { const auto k=cellText(row[0]), v=cellText(row[1]); if(k=="credit.monthly_rate_percent") rate=std::stod(v); else if(k=="credit.due_day") dueDay=std::stoi(v); else if(k=="credit.minimum_payment_percent") minimum=std::stod(v); } catch (...) { result.message="Cấu hình tín dụng không hợp lệ."; return result; }
    }
    const json appResponse = db.execute("SELECT a.user_id,a.requested_limit_vnd,a.monthly_income_vnd,COALESCE(d.status,'MISSING') FROM credit_applications a LEFT JOIN credit_application_documents d ON d.application_id=a.application_id WHERE a.application_id=? AND a.status='PENDING' LIMIT 1;", json::array({text(request.application_id)}));
    if (hasError(appResponse)) { result.message="Không đọc được hồ sơ cần duyệt."; return result; }
    const auto apps = rowsOf(appResponse);
    if (!apps.is_array() || apps.empty()) { result.message="Hồ sơ không tồn tại hoặc đã được xử lý."; return result; }
    if (request.action == "APPROVE" && cellText(apps[0][3]) != "APPROVED") {
        result.message = "Chỉ có thể duyệt hạn mức sau khi giấy tờ chứng minh thu nhập đã được xác nhận."; return result;
    }
    const std::string customerId=cellText(apps[0][0]);
    int64_t limit=request.approved_limit_vnd;
    int64_t requested=0, income=0;
    try { requested=std::stoll(cellText(apps[0][1])); income=std::stoll(cellText(apps[0][2])); if(request.action=="APPROVE" && limit==0) limit=requested; } catch (...) { result.message="Hạn mức yêu cầu sai định dạng."; return result; }
    if(request.action=="APPROVE" && (income < 1000000 || income > 10000000000LL || limit > requested || limit > std::min<int64_t>(500000000, income * 6))) { result.message="Hạn mức duyệt không được vượt số tiền khách đề nghị, 6 lần thu nhập đã khai báo hoặc 500.000.000 đồng. Hồ sơ cũ thiếu thu nhập cần gửi lại."; return result; }
    const std::string accountId="CRE_"+AuthSecurity::generateNumericCode(24);
    std::vector<std::pair<std::string,json>> batch;
    if (request.action == "REJECT") {
        batch.push_back({"UPDATE credit_applications SET status='REJECTED',decided_by=?,decision_note=?,decided_at=CURRENT_TIMESTAMP WHERE application_id=? AND status='PENDING';", json::array({text(session->user_id),text(request.note),text(request.application_id)})});
    } else {
        batch.push_back({"UPDATE credit_applications SET status='APPROVED',decided_by=?,decision_note=?,account_id=?,decided_at=CURRENT_TIMESTAMP WHERE application_id=? AND status='PENDING';", json::array({text(session->user_id),text(request.note),text(accountId),text(request.application_id)})});
        batch.push_back({"INSERT INTO accounts(account_id,user_id,balance_vnd,account_type,status,interest_rate,created_at) SELECT ?,?,0,'CREDIT','ACTIVE',?,CURRENT_TIMESTAMP WHERE changes()=1;", json::array({text(accountId),text(customerId),json{{"type","numeric"},{"value",rate}}})});
        batch.push_back({"INSERT INTO credit_lines(account_id,credit_limit_vnd,monthly_rate_percent,due_day,minimum_payment_percent,last_interest_date) SELECT ?,?,?,?,?,date('now') WHERE changes()=1;", json::array({text(accountId),number(limit),json{{"type","numeric"},{"value",rate}},number(dueDay),json{{"type","numeric"},{"value",minimum}}})});
    }
    const std::string auditId="CRLOG_"+AuthSecurity::generateNumericCode(20);
    batch.push_back({"INSERT INTO audit_logs(log_id,operator_id,action,target_id,details,created_at) SELECT ?,?,?,?, ?,CURRENT_TIMESTAMP WHERE changes()=1;",
        json::array({text(auditId),text(session->user_id),text(request.action=="APPROVE"?"CREDIT_LIMIT_APPROVED":"CREDIT_LIMIT_REJECTED"),text(request.application_id),text(request.action=="APPROVE"?"Approved limit: "+std::to_string(limit):request.note)})});
    const auto saved=db.executeBatch(batch);
    if(hasError(saved)){result.message="Không lưu được quyết định hoặc hạn mức vào database.";return result;}
    const auto verify=db.execute("SELECT status FROM credit_applications WHERE application_id=?;",json::array({text(request.application_id)}));
    const auto verifiedRows=rowsOf(verify);
    if(verifiedRows.empty()||cellText(verifiedRows[0][0])!=(request.action=="APPROVE"?"APPROVED":"REJECTED")){result.message="Database chưa xác nhận quyết định tín dụng.";return result;}
    return getApplications({request.session_token});
}

TransactionResult CreditService::spend(const CreditActionRequest& r) const {
    TransactionResult out;
    const auto session=sessions.getSession(r.session_token); const auto user=session?users.findById(session->user_id):nullptr;
    if(!session||session->role!="CUSTOMER"||!user||!user->isActive()){out.message="Phiên khách hàng không hợp lệ.";return out;}
    if(!user->verifyPin(r.pin)){out.message="Mã PIN giao dịch không chính xác.";return out;}
    if(r.amount_vnd<=0||r.amount_vnd>50000000||r.account_id.empty()||r.idempotency_key.empty()||r.receiver_account_id.empty()){out.message="Mỗi lần chi tiêu tín dụng tối đa 50.000.000 đồng; hãy nhập lại thông tin.";return out;}
    if(!ensureSchema()){out.message="Không truy cập được hạn mức tín dụng.";return out;}
    if(!accrueInterest(r.account_id,session->user_id)){out.message="Không cập nhật được lãi tín dụng.";return out;}
    const auto cached=db.execute("SELECT user_id,action,response_json FROM idempotency_keys WHERE idempotency_key=? LIMIT 1;",json::array({text(session->user_id+":CREDIT_SPEND:"+r.idempotency_key)}));
    if(!hasError(cached)&&!rowsOf(cached).empty()) { try { const auto& row=rowsOf(cached)[0]; if(cellText(row[0])==session->user_id&&cellText(row[1])=="CREDIT_SPEND") return json::parse(cellText(row[2])).get<TransactionResult>(); } catch (...) { out.message="Không đọc được kết quả giao dịch đã gửi."; return out; } }
    if(r.receiver_bank!="OOPBANK"){out.message="Chuyển tín dụng liên ngân hàng chưa được tích hợp. Hiện chỉ hỗ trợ tài khoản OOPBank.";return out;}
    const std::string receiver=r.receiver_account_id;
    if(r.receiver_account_id.size()<8||r.receiver_account_id.size()>16||!std::all_of(r.receiver_account_id.begin(),r.receiver_account_id.end(),[](unsigned char c){return std::isdigit(c)!=0;})){out.message="Số tài khoản nhận phải gồm 8–16 chữ số.";return out;}
    const auto id="TXN_"+AuthSecurity::generateNumericCode(24);
    const int64_t postBalance=0;
    out={true,"Chi tiêu tín dụng thành công.",id,postBalance};
    const std::string response=json(out).dump();
    std::vector<std::pair<std::string,json>> batch;
    const std::string idempotencyId=session->user_id+":CREDIT_SPEND:"+r.idempotency_key;
    const std::string reservation="PENDING:"+id;
    batch.push_back({"INSERT INTO idempotency_keys(idempotency_key,user_id,action,response_json,created_at) VALUES(?,?,'CREDIT_SPEND',?,CURRENT_TIMESTAMP) ON CONFLICT(idempotency_key) DO NOTHING;",json::array({text(idempotencyId),text(session->user_id),text(reservation)})});
    const std::string update="UPDATE accounts SET balance_vnd=balance_vnd+?,last_tx_id=? WHERE account_id=? AND user_id=? AND account_type='CREDIT' AND status='ACTIVE' AND balance_vnd+? <= (SELECT credit_limit_vnd FROM credit_lines WHERE account_id=?) AND EXISTS(SELECT 1 FROM accounts receiver WHERE receiver.account_id=? AND receiver.account_type='CHECKING' AND receiver.status='ACTIVE');";
    batch.push_back({update+" AND changes()=1 AND EXISTS(SELECT 1 FROM idempotency_keys WHERE idempotency_key=? AND response_json=?);",json::array({number(r.amount_vnd),text(id),text(r.account_id),text(session->user_id),number(r.amount_vnd),text(r.account_id),text(receiver),text(idempotencyId),text(reservation)})});
    batch.push_back({"UPDATE accounts SET balance_vnd=balance_vnd+?,last_tx_id=? WHERE account_id=? AND account_type='CHECKING' AND status='ACTIVE' AND changes()=1;",json::array({number(r.amount_vnd),text(id),text(receiver)})});
    const std::string txSql="INSERT INTO transactions(transaction_id,sender_account_id,receiver_account_id,amount_vnd,fee_vnd,type,status,description,post_balance_vnd,created_at) SELECT ?,?,?,?,0,'CREDIT_SPEND','SUCCESS',?,(SELECT balance_vnd FROM accounts WHERE account_id=?),CURRENT_TIMESTAMP WHERE changes()=1;";
    // For an internal transfer the preceding guarded receiver update proves the source update succeeded.
    batch.push_back({txSql,json::array({text(id),text(r.account_id),text(receiver),number(r.amount_vnd),text(r.description),text(r.account_id)})});
    batch.push_back({"UPDATE idempotency_keys SET response_json=? WHERE idempotency_key=? AND response_json=? AND EXISTS(SELECT 1 FROM transactions WHERE transaction_id=?);",json::array({text(response),text(idempotencyId),text(reservation),text(id)})});
    batch.push_back({"DELETE FROM idempotency_keys WHERE idempotency_key=? AND response_json=? AND NOT EXISTS(SELECT 1 FROM transactions WHERE transaction_id=?);",json::array({text(idempotencyId),text(reservation),text(id)})});
    const auto saved=db.executeBatch(batch);
    const auto verify=db.execute("SELECT transaction_id,post_balance_vnd FROM transactions WHERE transaction_id=? AND status='SUCCESS';",json::array({text(id)}));
    const auto rows=rowsOf(verify);
    if(hasError(saved)||rows.empty()){
        const auto duplicate=db.execute("SELECT response_json FROM idempotency_keys WHERE idempotency_key=? LIMIT 1;",json::array({text(idempotencyId)}));
        try { if(!hasError(duplicate)&&!rowsOf(duplicate).empty()&&cellText(rowsOf(duplicate)[0][0]).rfind("PENDING:",0)!=0) return json::parse(cellText(rowsOf(duplicate)[0][0])).get<TransactionResult>(); } catch (...) {}
        out={false,"Giao dịch tín dụng chưa được ghi nhận; không trừ tiền.","",0};return out;
    }
    out.post_balance_vnd=std::stoll(cellText(rows[0][1]));
    db.execute("UPDATE idempotency_keys SET response_json=? WHERE idempotency_key=? AND response_json=?;",json::array({text(json(out).dump()),text(idempotencyId),text(response)}));
    return out;
}

TransactionResult CreditService::repay(const CreditActionRequest& r) const {
    TransactionResult out;
    const auto session=sessions.getSession(r.session_token); const auto user=session?users.findById(session->user_id):nullptr;
    if(!session||session->role!="CUSTOMER"||!user||!user->isActive()){out.message="Phiên khách hàng không hợp lệ.";return out;}
    if(!user->verifyPin(r.pin)){out.message="Mã PIN giao dịch không chính xác.";return out;}
    if(r.amount_vnd<=0||r.amount_vnd>500000000||r.account_id.empty()||r.source_account_id.empty()||r.idempotency_key.empty()){out.message="Thông tin trả nợ tín dụng không hợp lệ.";return out;}
    if(!ensureSchema()){out.message="Không truy cập được dữ liệu tín dụng.";return out;}
    if(!accrueInterest(r.account_id,session->user_id)){out.message="Không cập nhật được lãi tín dụng.";return out;}
    const auto cached=db.execute("SELECT user_id,action,response_json FROM idempotency_keys WHERE idempotency_key=? LIMIT 1;",json::array({text(session->user_id+":CREDIT_REPAYMENT:"+r.idempotency_key)}));
    if(!hasError(cached)&&!rowsOf(cached).empty()) { try { const auto& row=rowsOf(cached)[0]; if(cellText(row[0])==session->user_id&&cellText(row[1])=="CREDIT_REPAYMENT") return json::parse(cellText(row[2])).get<TransactionResult>(); } catch (...) { out.message="Không đọc được kết quả giao dịch đã gửi."; return out; } }
    const auto checking=db.execute("SELECT account_id FROM accounts WHERE account_id=? AND user_id=? AND account_type='CHECKING' AND status='ACTIVE' LIMIT 1;",json::array({text(r.source_account_id),text(session->user_id)}));
    if(hasError(checking)||rowsOf(checking).empty()){out.message="Tài khoản nguồn không phải tài khoản thanh toán đang hoạt động của bạn.";return out;}
    const std::string source=r.source_account_id;
    const auto terms=db.execute("SELECT a.balance_vnd,c.minimum_payment_percent,c.due_day,CASE WHEN CAST(strftime('%d','now') AS INTEGER)>=c.due_day THEN 1 ELSE 0 END FROM accounts a JOIN credit_lines c ON c.account_id=a.account_id WHERE a.account_id=? AND a.user_id=? AND a.account_type='CREDIT' AND a.status='ACTIVE' LIMIT 1;",json::array({text(r.account_id),text(session->user_id)}));
    if(hasError(terms)||rowsOf(terms).empty()){out.message="Không tìm thấy hạn mức tín dụng đang hoạt động.";return out;}
    try {
        const auto& row=rowsOf(terms)[0];
        const int64_t debt=std::stoll(cellText(row[0]));
        if(cellText(row[3])=="1"&&debt>0) {
            const auto paidResult=db.execute("SELECT COALESCE(SUM(amount_vnd),0) FROM transactions WHERE receiver_account_id=? AND type='CREDIT_REPAYMENT' AND date(created_at)>=date('now','start of month');",json::array({text(r.account_id)}));
            if(hasError(paidResult)||rowsOf(paidResult).empty()){out.message="Không xác minh được khoản thanh toán tối thiểu đã trả trong kỳ.";return out;}
            const int64_t paid=std::stoll(cellText(rowsOf(paidResult)[0][0]));
            const double pct=std::stod(cellText(row[1]));
            const int64_t minimum=std::max<int64_t>(1000,static_cast<int64_t>((debt+paid)*pct/100.0));
            const int64_t remaining=std::max<int64_t>(0,minimum-paid);
            if(remaining>0&&r.amount_vnd<remaining){out.message="Đã đến ngày thanh toán. Số tiền cần trả ít nhất trong giao dịch này là "+std::to_string(remaining)+" đồng.";return out;}
        }
    } catch (...) {out.message="Không tính được khoản thanh toán tối thiểu.";return out;}
    const auto id="TXN_"+AuthSecurity::generateNumericCode(24); out={true,"Đã trả nợ tín dụng.",id,0};
    const std::string response=json(out).dump();
    const std::string idempotencyId=session->user_id+":CREDIT_REPAYMENT:"+r.idempotency_key;
    const std::string reservation="PENDING:"+id;
    std::vector<std::pair<std::string,json>> batch={
        {"INSERT INTO idempotency_keys(idempotency_key,user_id,action,response_json,created_at) VALUES(?,?,'CREDIT_REPAYMENT',?,CURRENT_TIMESTAMP) ON CONFLICT(idempotency_key) DO NOTHING;",json::array({text(idempotencyId),text(session->user_id),text(reservation)})},
        {"UPDATE accounts SET balance_vnd=balance_vnd-?,last_tx_id=? WHERE account_id=? AND user_id=? AND account_type='CHECKING' AND status='ACTIVE' AND balance_vnd>=? AND EXISTS(SELECT 1 FROM accounts c WHERE c.account_id=? AND c.user_id=? AND c.account_type='CREDIT' AND c.status='ACTIVE' AND c.balance_vnd>=?) AND changes()=1 AND EXISTS(SELECT 1 FROM idempotency_keys WHERE idempotency_key=? AND response_json=?);",json::array({number(r.amount_vnd),text(id),text(source),text(session->user_id),number(r.amount_vnd),text(r.account_id),text(session->user_id),number(r.amount_vnd),text(idempotencyId),text(reservation)})},
        {"UPDATE accounts SET balance_vnd=balance_vnd-?,last_tx_id=? WHERE account_id=? AND user_id=? AND account_type='CREDIT' AND status='ACTIVE' AND balance_vnd>=? AND changes()=1;",json::array({number(r.amount_vnd),text(id),text(r.account_id),text(session->user_id),number(r.amount_vnd)})},
        {"INSERT INTO transactions(transaction_id,sender_account_id,receiver_account_id,amount_vnd,fee_vnd,type,status,description,post_balance_vnd,created_at) SELECT ?,?,?,?,0,'CREDIT_REPAYMENT','SUCCESS',?,(SELECT balance_vnd FROM accounts WHERE account_id=?),CURRENT_TIMESTAMP WHERE changes()=1;",json::array({text(id),text(source),text(r.account_id),number(r.amount_vnd),text(r.description),text(r.account_id)})},
        {"UPDATE idempotency_keys SET response_json=? WHERE idempotency_key=? AND response_json=? AND EXISTS(SELECT 1 FROM transactions WHERE transaction_id=?);",json::array({text(response),text(idempotencyId),text(reservation),text(id)})},
        {"DELETE FROM idempotency_keys WHERE idempotency_key=? AND response_json=? AND NOT EXISTS(SELECT 1 FROM transactions WHERE transaction_id=?);",json::array({text(idempotencyId),text(reservation),text(id)})}
    };
    const auto saved=db.executeBatch(batch);
    const auto verify=db.execute("SELECT transaction_id,post_balance_vnd FROM transactions WHERE transaction_id=? AND status='SUCCESS';",json::array({text(id)}));
    const auto rows=rowsOf(verify);
    if(hasError(saved)||rows.empty()){
        const auto duplicate=db.execute("SELECT response_json FROM idempotency_keys WHERE idempotency_key=? LIMIT 1;",json::array({text(idempotencyId)}));
        try { if(!hasError(duplicate)&&!rowsOf(duplicate).empty()&&cellText(rowsOf(duplicate)[0][0]).rfind("PENDING:",0)!=0) return json::parse(cellText(rowsOf(duplicate)[0][0])).get<TransactionResult>(); } catch (...) {}
        out={false,"Không thể trả nợ: kiểm tra số dư thanh toán và dư nợ hiện tại.","",0};return out;
    }
    out.post_balance_vnd=std::stoll(cellText(rows[0][1]));
    db.execute("UPDATE idempotency_keys SET response_json=? WHERE idempotency_key=? AND response_json=?;",json::array({text(json(out).dump()),text(idempotencyId),text(response)}));
    return out;
}
}
