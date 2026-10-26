#include "services/LoansService.hpp"
#include "core/AuthSecurity.hpp"
#include <QDate>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>

namespace Banking {

namespace {
std::string loanCellText(const json& cell) {
    if (!cell.is_object() || !cell.contains("value") || cell["value"].is_null()) return {};
    const auto& value = cell["value"];
    return value.is_string() ? value.get<std::string>() : value.dump();
}

int64_t loanCellInt64(const json& cell) {
    const std::string value = loanCellText(cell);
    return value.empty() ? 0 : std::stoll(value);
}
}

LoanService::LoanService(LoanRepository& loanRepo, AccountRepository& accRepo,
                         UserRepository& userRepo, AuditRepository& auditRepo,
                         SessionManager& sessionManager, TursoDB& tursoDb)
    : m_loanRepo(loanRepo), m_accRepo(accRepo), m_userRepo(userRepo),
      m_auditRepo(auditRepo), m_sessionManager(sessionManager), m_tursoDb(tursoDb) {}

// Đọc các khoản vay thuộc session hiện tại để nạp vào giao diện khách hàng.
std::vector<LoanModel> LoanService::getLoansForSession(const std::string& sessionToken) {
    const auto session = m_sessionManager.getSession(sessionToken);
    if (!session.has_value()) return {};
    return m_loanRepo.findByUserId(session->user_id);
}

StaffLoanApplicationsResult LoanService::getApplicationsForStaff(const std::string& sessionToken) {
    StaffLoanApplicationsResult result;
    const auto session = m_sessionManager.getSession(sessionToken);
    if (!session.has_value() || (session->role != "TELLER" && session->role != "ADMIN")) {
        result.message = "Bạn không có quyền xem danh sách hồ sơ vay.";
        return result;
    }

    for (const LoanModel& loan : m_loanRepo.findAll()) {
        StaffLoanApplicationDTO item;
        item.loan_id = loan.getLoanId();
        item.user_id = loan.getUserId();
        item.amount_vnd = loan.getOriginalAmountVnd();
        item.interest_rate = loan.getAnnualInterestRate();
        item.term_months = loan.getTermMonths();
        item.calc_method = loan.getCalcMethod();
        item.collateral_info = loan.getCollateralInfo();
        item.purpose = loan.getPurpose();
        item.monthly_income_vnd = loan.getMonthlyIncomeVnd();
        item.status = loan.getStatus();
        item.created_at = loan.getCreatedAt();
        const auto customer = m_userRepo.findById(loan.getUserId());
        item.customer_name = customer ? customer->getFullName() : loan.getUserId();
        item.customer_phone = customer ? customer->getPhone() : "";
        result.applications.push_back(std::move(item));
    }

    result.success = true;
    result.message = "Đã tải danh sách hồ sơ vay.";
    return result;
}

json LoanService::getAvailableTerms() {
    const json defaults = json::array({
        {{"months", 3}, {"enabled", true}, {"rate", 10.5}},
        {{"months", 6}, {"enabled", true}, {"rate", 11.0}},
        {{"months", 12}, {"enabled", true}, {"rate", 12.0}}
    });
    const json response = m_tursoDb.execute(
        "SELECT value FROM system_config WHERE key = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", "loan.terms"}}}));
    if (!response.contains("results") || !response["results"].is_array() || response["results"].empty() ||
        !response["results"][0].is_object() || response["results"][0].value("type", "") != "ok") {
        throw std::runtime_error("Không thể tải kỳ hạn vay từ database.");
    }
    const auto& rows = response["results"][0]["response"]["result"]["rows"];
    if (!rows.is_array()) throw std::runtime_error("Database trả về cấu hình kỳ hạn vay không hợp lệ.");
    if (rows.empty()) return defaults;
    if (!rows[0].is_array() || rows[0].empty() || !rows[0][0].is_object() ||
        !rows[0][0].contains("value") || rows[0][0]["value"].is_null()) {
        throw std::runtime_error("Database trả về cấu hình kỳ hạn vay không đầy đủ.");
    }
    const auto& cell = rows[0][0]["value"];
    const std::string encoded = cell.is_string() ? cell.get<std::string>() : cell.dump();
    const json terms = json::parse(encoded);
    if (!terms.is_array() || terms.empty()) {
        throw std::runtime_error("Cấu hình kỳ hạn vay trong database không hợp lệ.");
    }
    return terms;
}

LoanDocumentsResult LoanService::getDocuments(const std::string& sessionToken, const std::string& loanId) {
    LoanDocumentsResult result;
    const auto session = m_sessionManager.getSession(sessionToken);
    if (!session) { result.message = "Phiên đăng nhập đã hết hạn."; return result; }
    if (session->role == "CUSTOMER") {
        const auto loan = m_loanRepo.findById(loanId);
        if (!loan || loan->getUserId() != session->user_id) { result.message = "Không tìm thấy hồ sơ vay."; return result; }
    } else if (session->role != "TELLER" && session->role != "ADMIN") {
        result.message = "Không có quyền xem tài liệu hồ sơ vay.";
        return result;
    }
    const json response = m_tursoDb.execute(
        "SELECT document_id, loan_id, document_type, file_name, mime_type, file_size, status, review_note, submitted_at, storage_key "
        "FROM loan_documents WHERE loan_id = ? ORDER BY document_type;",
        json::array({{{"type", "text"}, {"value", loanId}}}));
    if (!response.contains("results") || !response["results"].is_array() || response["results"].empty() ||
        !response["results"][0].is_object() || response["results"][0].value("type", "") != "ok") {
        result.message = "Không tải được danh sách tài liệu vay.";
        return result;
    }
    try {
        const auto rows = response["results"][0]["response"]["result"]["rows"];
        if (!rows.is_array()) throw std::runtime_error("invalid document rows");
        auto cell = [](const json& value) {
            if (!value.is_object() || !value.contains("value") || value["value"].is_null()) return std::string{};
            return value["value"].is_string() ? value["value"].get<std::string>() : value["value"].dump();
        };
        for (const auto& row : rows) {
            if (!row.is_array() || row.size() < 10) throw std::runtime_error("incomplete document row");
            result.documents.push_back({cell(row[0]), cell(row[1]), cell(row[2]), cell(row[3]), cell(row[4]),
                std::stoll(cell(row[5])), cell(row[6]), cell(row[7]), cell(row[8]), cell(row[9])});
        }
        result.success = true;
        result.message = "Đã tải danh sách tài liệu.";
    } catch (...) { result.message = "Dữ liệu tài liệu vay không hợp lệ."; }
    return result;
}

LoanOperationResult LoanService::submitDocument(const SubmitLoanDocumentRequest& request) {
    LoanOperationResult result;
    const auto session = m_sessionManager.getSession(request.session_token);
    if (!session || session->role != "CUSTOMER") { result.message = "Chỉ khách hàng đang đăng nhập mới được nộp tài liệu."; return result; }
    const auto loan = m_loanRepo.findById(request.loan_id);
    if (!loan || loan->getUserId() != session->user_id || loan->getStatus() != "PENDING") {
        result.message = "Không tìm thấy hồ sơ đang chờ bổ sung tài liệu.";
        return result;
    }
    const std::string customPrefix = "CUSTOM:";
    const std::string customName = request.document_type.rfind(customPrefix, 0) == 0
        ? request.document_type.substr(customPrefix.size()) : std::string{};
    const bool customTypeAllowed = !customName.empty() && customName.size() <= 80 &&
        customName.find_first_not_of(" \t\r\n") != std::string::npos &&
        customName.find_first_of("\r\n") == std::string::npos;
    const bool typeAllowed = request.document_type == "cccd" || request.document_type == "income" ||
                             request.document_type == "collateral" || request.document_type == "address" ||
                             customTypeAllowed;
    const bool mimeAllowed = request.mime_type == "application/pdf" || request.mime_type == "image/jpeg" || request.mime_type == "image/png";
    if (!typeAllowed || !mimeAllowed || request.file_name.empty() || request.file_name.size() > 255 ||
        request.file_name.find_first_of("\r\n") != std::string::npos || request.file_size <= 0 ||
        request.file_size > 5 * 1024 * 1024 || request.storage_key.empty()) {
        result.message = "Tài liệu phải là PDF/JPG/PNG và tối đa 5 MB.";
        return result;
    }
    const std::string storageKey = request.storage_key;
    const std::string allowedCloudinaryPrefix = "https://res.cloudinary.com/ltcqqgmq/";
    if (storageKey.rfind(allowedCloudinaryPrefix, 0) != 0) {
        result.message = "Liên kết tài liệu Cloudinary không hợp lệ.";
        return result;
    }
    const std::string documentId = "DOC_" + AuthSecurity::generateNumericCode(24);
    const json saved = m_tursoDb.execute(
        "INSERT INTO loan_documents (document_id, loan_id, user_id, document_type, file_name, mime_type, file_size, storage_key, status, review_note, reviewed_by, submitted_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'PENDING_REVIEW', '', NULL, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP) "
        "ON CONFLICT(loan_id, document_type) DO UPDATE SET file_name=excluded.file_name, mime_type=excluded.mime_type, file_size=excluded.file_size, storage_key=excluded.storage_key, status='PENDING_REVIEW', review_note='', reviewed_by=NULL, submitted_at=CURRENT_TIMESTAMP, updated_at=CURRENT_TIMESTAMP;",
        json::array({{{"type", "text"}, {"value", documentId}}, {{"type", "text"}, {"value", request.loan_id}},
                     {{"type", "text"}, {"value", session->user_id}}, {{"type", "text"}, {"value", request.document_type}},
                     {{"type", "text"}, {"value", request.file_name}}, {{"type", "text"}, {"value", request.mime_type}},
                     {{"type", "numeric"}, {"value", request.file_size}}, {{"type", "text"}, {"value", storageKey}}}));
    if (!saved.contains("results") || saved["results"].empty() || saved["results"][0].value("type", "") != "ok") {
        result.message = "Tài liệu đã được gửi nhưng không lưu được metadata database. Hãy liên hệ hỗ trợ trước khi gửi lại.";
        return result;
    }
    result.success = true;
    result.loan_id = request.loan_id;
    result.message = "Đã nộp tài liệu để ngân hàng kiểm tra.";
    return result;
}

LoanOperationResult LoanService::reviewDocument(const ReviewLoanDocumentRequest& request) {
    LoanOperationResult result;
    const auto session = m_sessionManager.getSession(request.session_token);
    if (!session || (session->role != "TELLER" && session->role != "ADMIN")) {
        result.message = "Chỉ nhân viên được phân quyền mới được kiểm tra tài liệu.";
        return result;
    }

    if (request.action == "REQUEST_ADDITIONAL_DOCUMENT") {
        const std::string documentName = request.document_name;
        const auto firstVisible = documentName.find_first_not_of(" \t\r\n");
        const auto lastVisible = documentName.find_last_not_of(" \t\r\n");
        if (request.loan_id.empty() || firstVisible == std::string::npos ||
            request.note.find_first_not_of(" \t\r\n") == std::string::npos ||
            lastVisible - firstVisible + 1 > 80 || documentName.find_first_of("\r\n") != std::string::npos) {
            result.message = "Tên giấy tờ bổ sung và lý do yêu cầu không hợp lệ.";
            return result;
        }
        const auto loan = m_loanRepo.findById(request.loan_id);
        if (!loan || loan->getStatus() != "PENDING") {
            result.message = "Chỉ có thể yêu cầu giấy tờ bổ sung cho hồ sơ đang chờ xử lý.";
            return result;
        }

        const std::string trimmedName = documentName.substr(firstVisible, lastVisible - firstVisible + 1);
        const std::string documentType = "CUSTOM:" + trimmedName;
        const std::string documentId = "DOC_" + AuthSecurity::generateNumericCode(24);
        const json inserted = m_tursoDb.execute(
            "INSERT INTO loan_documents (document_id, loan_id, user_id, document_type, file_name, mime_type, file_size, storage_key, status, review_note, reviewed_by, submitted_at, updated_at) "
            "VALUES (?, ?, ?, ?, '', '', 0, '', 'MISSING', ?, NULL, NULL, CURRENT_TIMESTAMP) "
            "ON CONFLICT(loan_id, document_type) DO NOTHING RETURNING document_id;",
            json::array({{{"type", "text"}, {"value", documentId}},
                         {{"type", "text"}, {"value", request.loan_id}},
                         {{"type", "text"}, {"value", loan->getUserId()}},
                         {{"type", "text"}, {"value", documentType}},
                         {{"type", "text"}, {"value", request.note}}}));
        try {
            const auto rows = inserted["results"][0]["response"]["result"]["rows"];
            if (!rows.is_array() || rows.empty()) {
                result.message = "Giấy tờ này đã có trong hồ sơ; hãy yêu cầu khách nộp lại mục hiện có.";
                return result;
            }
        } catch (...) {
            result.message = "Không lưu được giấy tờ bổ sung vào hồ sơ.";
            return result;
        }

        const std::string notificationId = "NTF_" + AuthSecurity::generateNumericCode(20);
        const json notice = m_tursoDb.execute(
            "INSERT INTO notifications (notification_id, user_id, title, body, notification_type, reference_id) VALUES (?, ?, ?, ?, 'LOAN', ?);",
            json::array({{{"type", "text"}, {"value", notificationId}},
                         {{"type", "text"}, {"value", loan->getUserId()}},
                         {{"type", "text"}, {"value", "Ngân hàng yêu cầu bổ sung tài liệu"}},
                         {{"type", "text"}, {"value", request.note}},
                         {{"type", "text"}, {"value", request.loan_id}}}));
        m_auditRepo.logAction(session->user_id, "REQUEST_ADDITIONAL_LOAN_DOCUMENT", request.loan_id,
                              "Requested document: " + trimmedName);
        result.success = true;
        result.loan_id = request.loan_id;
        result.message = notice.contains("results") && !notice["results"].empty() &&
                         notice["results"][0].value("type", "") == "ok"
            ? "Đã lưu giấy tờ cần bổ sung và gửi thông báo cho khách hàng."
            : "Đã lưu giấy tờ cần bổ sung nhưng chưa gửi được thông báo.";
        return result;
    }

    const bool accept = request.action == "APPROVE";
    if (accept && session->role != "ADMIN") {
        result.message = "Giao dịch viên chỉ được xem tài liệu và yêu cầu bổ sung; xác nhận thuộc Admin.";
        return result;
    }
    if ((!accept && request.action != "REQUEST_RESUBMISSION") ||
        (!accept && request.note.find_first_not_of(" \t\r\n") == std::string::npos)) {
        result.message = "Thao tác hoặc lý do yêu cầu bổ sung không hợp lệ.";
        return result;
    }
    const json response = m_tursoDb.execute(
        accept
        ? "UPDATE loan_documents SET status = 'APPROVED', review_note = ?, reviewed_by = ?, updated_at = CURRENT_TIMESTAMP WHERE document_id = ? AND status = 'PENDING_REVIEW' AND EXISTS (SELECT 1 FROM loans WHERE loans.loan_id = loan_documents.loan_id AND loans.status = 'PENDING') RETURNING loan_id, user_id;"
        : "UPDATE loan_documents SET status = 'REJECTED', review_note = ?, reviewed_by = ?, updated_at = CURRENT_TIMESTAMP WHERE document_id = ? AND status IN ('PENDING_REVIEW','MISSING','REJECTED') AND EXISTS (SELECT 1 FROM loans WHERE loans.loan_id = loan_documents.loan_id AND loans.status = 'PENDING') RETURNING loan_id, user_id;",
        json::array({{{"type", "text"}, {"value", request.note}}, {{"type", "text"}, {"value", session->user_id}},
                     {{"type", "text"}, {"value", request.document_id}}}));
    try {
        const auto rows = response["results"][0]["response"]["result"]["rows"];
        if (!rows.is_array() || rows.empty()) { result.message = "Tài liệu không tồn tại hoặc đã được xử lý."; return result; }
        result.success = true;
        result.loan_id = rows[0][0].value("value", std::string{});
        result.message = accept ? "Đã xác nhận tài liệu." : "Đã yêu cầu khách hàng nộp lại tài liệu.";
        if (!accept) {
            const std::string notificationId = "NTF_" + AuthSecurity::generateNumericCode(20);
            const json notice = m_tursoDb.execute(
                "INSERT INTO notifications (notification_id, user_id, title, body, notification_type, reference_id) VALUES (?, ?, ?, ?, 'LOAN', ?);",
                json::array({{{"type", "text"}, {"value", notificationId}},
                             {{"type", "text"}, {"value", rows[0][1].value("value", std::string{})}},
                             {{"type", "text"}, {"value", "Ngân hàng yêu cầu bổ sung tài liệu"}},
                             {{"type", "text"}, {"value", request.note}},
                             {{"type", "text"}, {"value", result.loan_id}}}));
            if (!notice.contains("results") || notice["results"].empty() ||
                notice["results"][0].value("type", "") != "ok") {
                result.message = "Đã lưu yêu cầu nộp lại nhưng chưa gửi được thông báo.";
            }
        }
    } catch (...) { result.message = "Không cập nhật được trạng thái tài liệu."; }
    return result;
}

LoanDocumentLinkResult LoanService::getDocumentLink(const LoanDocumentLinkRequest& request) {
    LoanDocumentLinkResult result;
    const auto session = m_sessionManager.getSession(request.session_token);
    if (!session) { result.message = "Phiên đăng nhập đã hết hạn."; return result; }
    const json lookup = m_tursoDb.execute(
        "SELECT loan_id, user_id, storage_key FROM loan_documents WHERE document_id = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", request.document_id}}}));
    try {
        const auto row = lookup["results"][0]["response"]["result"]["rows"][0];
        auto text = [](const json& cell) { return cell.value("value", json()).is_string() ? cell.value("value", json()).get<std::string>() : cell.value("value", json()).dump(); };
        const std::string owner = text(row[1]);
        if (session->role == "CUSTOMER" && owner != session->user_id) { result.message = "Không có quyền xem tài liệu này."; return result; }
        if (session->role != "CUSTOMER" && session->role != "TELLER" && session->role != "ADMIN") { result.message = "Không có quyền xem tài liệu."; return result; }
        const std::string cloudinaryPrefix = "https://res.cloudinary.com/ltcqqgmq/";
        if (text(row[2]).rfind(cloudinaryPrefix, 0) == 0) {
            result.url = text(row[2]);
            result.success = true;
            result.message = "Đã tạo liên kết xem tài liệu.";
            return result;
        }
        result.message = "Liên kết tài liệu không thuộc Cloudinary đã cấu hình.";
    } catch (...) { result.message = "Không tìm thấy tài liệu hoặc database không phản hồi."; }
    return result;
}

// 1. TẠO ĐƠN XIN VAY
LoanOperationResult LoanService::applyLoan(const ApplyLoanRequest& req) {
    LoanOperationResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value()) {
        res.message = "Phiên làm việc hết hạn!";
        return res;
    }
    const auto& sess = sessOpt.value();

    const bool staffSession = sess.role == "TELLER" || sess.role == "ADMIN";
    const std::string targetUserId = req.target_user_id.empty() ? sess.user_id : req.target_user_id;
    if (sess.role != "CUSTOMER" && !staffSession) {
        res.message = "Bạn không có quyền nộp đơn xin vay.";
        return res;
    }
    if (sess.role == "CUSTOMER" && targetUserId != sess.user_id) {
        res.message = "Khách hàng chỉ được nộp đơn vay cho chính mình.";
        return res;
    }
    if (staffSession && req.target_user_id.empty()) {
        res.message = "Thiếu thông tin khách hàng cần nộp hồ sơ vay.";
        return res;
    }
    if (staffSession && !req.kyc_verified) {
        res.message = "Cần xác nhận đã đối chiếu CCCD bản gốc trước khi nộp hồ sơ vay.";
        return res;
    }
    if (staffSession) {
        const auto customer = m_userRepo.findById(targetUserId);
        if (!customer || customer->getRole() != "CUSTOMER" || !customer->isActive()) {
            res.message = "Khách hàng không tồn tại hoặc không hoạt động.";
            return res;
        }
        if (!customer->verifyPin(req.customer_pin)) {
            res.message = "Mã PIN giao dịch của khách hàng không chính xác.";
            return res;
        }
    }
    const std::set<std::string> requiredDocs = {"cccd", "income", "collateral", "address"};
    const std::set<std::string> receivedDocs(req.received_document_types.begin(), req.received_document_types.end());
    if (std::any_of(receivedDocs.begin(), receivedDocs.end(), [&](const std::string& type) {
        return requiredDocs.find(type) == requiredDocs.end();
    })) {
        res.message = "Danh sách giấy tờ nhận tại quầy không hợp lệ.";
        return res;
    }
    if (req.amount_vnd <= 0 || req.term_months < 1 || req.term_months > 360 ||
        (req.calc_method != "FLAT" && req.calc_method != "DECREASING") || req.purpose.empty() ||
        req.purpose.size() > 300 || req.monthly_income_vnd <= 0 || req.monthly_income_vnd > 1000000000000LL) {
        res.message = "Vui lòng nhập số tiền, kỳ hạn, mục đích vay và thu nhập hợp lệ.";
        return res;
    }

    const std::string loanId = "LOAN_" + AuthSecurity::generateNumericCode(24);
    const json terms = getAvailableTerms();
    double configuredRate = 0.0;
    bool termEnabled = false;
    for (const auto& term : terms) {
        if (!term.is_object() || term.value("months", 0) != req.term_months) continue;
        termEnabled = term.value("enabled", false);
        configuredRate = term.value("rate", 0.0);
        break;
    }
    if (!termEnabled || configuredRate <= 0.0 || configuredRate > 50.0) {
        res.message = "Kỳ hạn vay này hiện không được mở hoặc có cấu hình lãi suất không hợp lệ.";
        return res;
    }

    const std::string sqlLoan =
        "INSERT INTO loans (loan_id, user_id, amount_vnd, interest_rate, term_months, "
        "calc_method, collateral_info, purpose, monthly_income_vnd, status, created_by, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'PENDING', ?, CURRENT_TIMESTAMP);";

    json argsLoan = json::array({
        {{"type", "text"},    {"value", loanId}},
        {{"type", "text"},    {"value", targetUserId}},
        {{"type", "numeric"}, {"value", req.amount_vnd}},
        {{"type", "numeric"}, {"value", configuredRate}},
        {{"type", "numeric"}, {"value", req.term_months}},
        {{"type", "text"},    {"value", req.calc_method}},
        {{"type", "text"},    {"value", req.collateral_info}},
        {{"type", "text"},    {"value", req.purpose}},
        {{"type", "numeric"}, {"value", req.monthly_income_vnd}},
        {{"type", "text"},    {"value", sess.user_id}}
    });

    std::vector<std::pair<std::string, json>> statements = {{sqlLoan, argsLoan}};
    for (const auto& type : requiredDocs) {
        const std::string documentId = "DOC_" + AuthSecurity::generateNumericCode(20);
        const bool received = staffSession && receivedDocs.find(type) != receivedDocs.end();
        statements.push_back({
            "INSERT INTO loan_documents (document_id, loan_id, user_id, document_type, status, submitted_at) VALUES (?, ?, ?, ?, ?, CASE WHEN ? = 1 THEN CURRENT_TIMESTAMP ELSE NULL END);",
            json::array({{{"type", "text"}, {"value", documentId}}, {{"type", "text"}, {"value", loanId}},
                         {{"type", "text"}, {"value", targetUserId}}, {{"type", "text"}, {"value", type}},
                         {{"type", "text"}, {"value", received ? "PENDING_REVIEW" : "MISSING"}},
                         {{"type", "numeric"}, {"value", received ? 1 : 0}}})
        });
    }
    const json dbRes = m_tursoDb.executeBatch(statements);
    bool saved = dbRes.contains("results") && dbRes["results"].is_array() && dbRes["results"].size() == statements.size();
    if (saved) for (const auto& item : dbRes["results"]) saved = saved && item.is_object() && item.value("type", "") == "ok";
    if (saved) {
        res.success = true;
        res.message = "Tạo đơn xin vay thành công!";
        res.loan_id = loanId;
        m_auditRepo.logAction(sess.user_id, "APPLY_LOAN_SUCCESS", loanId,
                              "Customer: " + targetUserId + ", amount: " + std::to_string(req.amount_vnd) + " VND");
        const std::string notificationId = "NTF_" + AuthSecurity::generateNumericCode(20);
        m_tursoDb.execute("INSERT INTO notifications (notification_id, user_id, title, body, notification_type, reference_id) VALUES (?, ?, ?, ?, 'LOAN', ?);",
            json::array({{{"type", "text"}, {"value", notificationId}}, {{"type", "text"}, {"value", targetUserId}},
                         {{"type", "text"}, {"value", "Đã tiếp nhận hồ sơ vay"}},
                         {{"type", "text"}, {"value", "Hồ sơ " + loanId + " đã được ghi nhận và đang chờ kiểm tra."}},
                         {{"type", "text"}, {"value", loanId}}}));
    } else {
        res.message = "Lỗi CSDL khi nộp đơn xin vay!";
    }

    return res;
}

// 2. PHÊ DUYỆT KHOẢN VAY (KÈM KIỂM TRA QUYỀN RBAC)
LoanOperationResult LoanService::approveLoan(const ApproveLoanRequest& req) {
    LoanOperationResult res;
    auto sessOpt = m_sessionManager.getSession(req.session_token);
    if (!sessOpt.has_value() || sessOpt->role != "ADMIN") {
        res.message = "BẢO MẬT: Bạn không có quyền phê duyệt khoản vay!";
        return res;
    }
    const auto& operatorSess = sessOpt.value();

    auto loanOpt = m_loanRepo.findById(req.loan_id);
    if (!loanOpt.has_value() ) {
        res.message = "Khoản vay không tồn tại hoặc đã được xử lý!";
        return res;
    }
    const auto& loan = loanOpt.value();

    // Teller may inspect and request documents; approval belongs to Admin.

    if (req.action != "APPROVE" && req.action != "REJECT") {
        res.message = "Quyết định khoản vay không hợp lệ.";
        return res;
    }
    if (loan.getStatus() != "PENDING") {
        res.message = "Khoản vay đã được xử lý trước đó.";
        return res;
    }
    if (req.action == "APPROVE") {
        const json documents = m_tursoDb.execute(
            "SELECT COUNT(CASE WHEN status = 'APPROVED' AND document_type IN ('cccd','income','collateral','address') THEN 1 END), "
            "COUNT(CASE WHEN status != 'APPROVED' THEN 1 END) FROM loan_documents WHERE loan_id = ?;",
            json::array({{{"type", "text"}, {"value", req.loan_id}}}));
        try {
            const auto rows = documents["results"][0]["response"]["result"]["rows"];
            if (!rows.is_array() || rows.empty() ||
                std::stoll(rows[0][0]["value"].get<std::string>()) < 4 ||
                std::stoll(rows[0][1]["value"].get<std::string>()) > 0) {
                res.message = "Chưa thể phê duyệt: cần xác nhận đủ 4 giấy tờ bắt buộc và mọi giấy tờ bổ sung.";
                return res;
            }
        } catch (...) { res.message = "Không xác minh được trạng thái tài liệu vay."; return res; }
    }
    const std::string newStatus = req.action == "APPROVE" ? "APPROVED" : "REJECTED";
    std::string sql = "UPDATE loans SET status = ?, approved_by = ? WHERE loan_id = ? AND status = 'PENDING';";
    json args = json::array({
        {{"type", "text"}, {"value", newStatus}},
        {{"type", "text"}, {"value", operatorSess.user_id}},
        {{"type", "text"}, {"value", req.loan_id}}
    });

    json queryRes = m_tursoDb.execute(sql, args);
    if (queryRes.contains("results") && !queryRes["results"].empty() &&
        queryRes["results"][0].is_object() && queryRes["results"][0].value("type", "") == "ok") {
        res.success = true;
        res.message = (newStatus == "APPROVED") ? "Đã phê duyệt khoản vay!" : "Đã từ chối đơn vay.";
        res.loan_id = req.loan_id;
        const std::string details = "Status: " + newStatus +
            (req.reason.empty() ? std::string{} : "; Reason: " + req.reason);
        m_auditRepo.logAction(operatorSess.user_id, "LOAN_DECISION", req.loan_id, details);
        const std::string notificationId = "NTF_" + AuthSecurity::generateNumericCode(20);
        const std::string title = newStatus == "APPROVED" ? "Hồ sơ vay đã được phê duyệt" : "Hồ sơ vay chưa được phê duyệt";
        const std::string body = newStatus == "APPROVED"
            ? "Hồ sơ " + req.loan_id + " đã được phê duyệt. Bạn có thể tiếp tục bước giải ngân."
            : "Hồ sơ " + req.loan_id + " bị từ chối. Lý do: " + (req.reason.empty() ? "Vui lòng liên hệ ngân hàng." : req.reason);
        const json notice = m_tursoDb.execute(
            "INSERT INTO notifications (notification_id, user_id, title, body, notification_type, reference_id) VALUES (?, ?, ?, ?, 'LOAN', ?);",
            json::array({{{"type", "text"}, {"value", notificationId}}, {{"type", "text"}, {"value", loan.getUserId()}},
                         {{"type", "text"}, {"value", title}}, {{"type", "text"}, {"value", body}},
                         {{"type", "text"}, {"value", req.loan_id}}}));
        if (!notice.contains("results") || notice["results"].empty() || notice["results"][0].value("type", "") != "ok")
            res.message += " Quyết định đã lưu nhưng chưa gửi được thông báo.";
    } else {
        res.message = "Lỗi CSDL khi cập nhật trạng thái phê duyệt!";
    }

    return res;
}

// 3. GIẢI NGÂN KHOẢN VAY
LoanOperationResult LoanService::disburseLoan(const DisburseLoanRequest& req) {
    LoanOperationResult res;
    const auto session = m_sessionManager.getSession(req.session_token);
    if (!session.has_value()) {
        res.message = "Phiên làm việc đã hết hạn!";
        return res;
    }
    if (session->role != "ADMIN") {
        res.message = "Chỉ Admin được quyền giải ngân khoản vay!";
        return res;
    }

    const auto loanOpt = m_loanRepo.findById(req.loan_id);
    if (!loanOpt.has_value() || loanOpt->getStatus() != "APPROVED") {
        res.message = "Khoản vay không tồn tại hoặc chưa được phê duyệt để giải ngân!";
        return res;
    }
    const LoanModel& loan = loanOpt.value();
    const auto accounts = m_accRepo.getAccountsByUserId(loan.getUserId());
    std::string targetAccountId = req.target_account_id;
    if (targetAccountId.empty()) {
        const auto firstChecking = std::find_if(accounts.begin(), accounts.end(), [](const AccountDTO& account) {
            return account.account_type == "CHECKING" && account.status == "ACTIVE";
        });
        if (firstChecking == accounts.end()) { res.message = "Người vay chưa có tài khoản thanh toán đang hoạt động."; return res; }
        targetAccountId = firstChecking->account_id;
    }

    if (session->role == "CUSTOMER") {
        if (session->user_id != loan.getUserId()) {
            res.message = "Khách hàng chỉ được giải ngân khoản vay của chính mình.";
            return res;
        }
        const auto user = m_userRepo.findById(session->user_id);
        if (!user || !user->verifyPin(req.pin)) {
            res.message = "Mã PIN giao dịch không chính xác!";
            return res;
        }
    }

    const auto target = std::find_if(accounts.begin(), accounts.end(), [&](const AccountDTO& account) {
        return account.account_id == targetAccountId &&
               account.account_type == "CHECKING" && account.status == "ACTIVE";
    });
    if (target == accounts.end()) {
        res.message = "Tài khoản nhận phải là tài khoản thanh toán đang hoạt động của người vay.";
        return res;
    }

    std::vector<std::pair<std::string, json>> batch;
    const std::string disbursementTransactionId = "TXN_" + AuthSecurity::generateNumericCode(24);
    const std::string notificationId = "NTF_" + AuthSecurity::generateNumericCode(20);
    batch.push_back({
        "UPDATE accounts SET balance_vnd = balance_vnd + ?, last_tx_id = ? WHERE account_id = ? AND user_id = ? AND account_type = 'CHECKING' AND status = 'ACTIVE' "
        "AND EXISTS (SELECT 1 FROM loans WHERE loan_id = ? AND status = 'APPROVED');",
        json::array({
            {{"type", "numeric"}, {"value", loan.getOriginalAmountVnd()}},
            {{"type", "text"}, {"value", disbursementTransactionId}},
            {{"type", "text"}, {"value", targetAccountId}},
            {{"type", "text"}, {"value", loan.getUserId()}},
            {{"type", "text"}, {"value", req.loan_id}}
        })
    });
    batch.push_back({
        "UPDATE loans SET status = 'DISBURSED', disbursed_account_id = ?, remaining_debt_vnd = amount_vnd WHERE loan_id = ? AND status = 'APPROVED' "
        "AND EXISTS (SELECT 1 FROM accounts WHERE account_id = ? AND user_id = loans.user_id AND account_type = 'CHECKING' AND status = 'ACTIVE') "
        "AND changes() > 0;",
        json::array({
            {{"type", "text"}, {"value", targetAccountId}},
            {{"type", "text"}, {"value", req.loan_id}},
            {{"type", "text"}, {"value", targetAccountId}}
        })
    });

    const auto schedule = loan.generateRepaymentSchedule();
    const QDate startDate = QDate::currentDate();
    for (const auto& item : schedule) {
        const std::string scheduleId = "SCH_" + AuthSecurity::generateNumericCode(22);
        const std::string dueDate = startDate.addMonths(item.period).toString(Qt::ISODate).toStdString();
        batch.push_back({
            "INSERT INTO loan_schedules (schedule_id, loan_id, period_num, due_date, principal_due_vnd, interest_due_vnd, status) "
            "SELECT ?, ?, ?, ?, ?, ?, 'UNPAID' WHERE changes() > 0 AND "
            "EXISTS (SELECT 1 FROM loans WHERE loan_id = ? AND status = 'DISBURSED');",
            json::array({{{"type", "text"}, {"value", scheduleId}}, {{"type", "text"}, {"value", req.loan_id}},
                         {{"type", "numeric"}, {"value", item.period}}, {{"type", "text"}, {"value", dueDate}},
                         {{"type", "numeric"}, {"value", item.principalVnd}}, {{"type", "numeric"}, {"value", item.interestVnd}},
                         {{"type", "text"}, {"value", req.loan_id}}})
        });
    }
    batch.push_back({
        "INSERT INTO transactions (transaction_id, sender_account_id, receiver_account_id, amount_vnd, fee_vnd, type, status, description, post_balance_vnd) "
        "SELECT ?, 'LOAN:' || l.loan_id, a.account_id, l.amount_vnd, 0, 'LOAN_DISBURSEMENT', 'SUCCESS', "
        "'Giải ngân khoản vay ' || l.loan_id, a.balance_vnd FROM loans l JOIN accounts a ON a.account_id = l.disbursed_account_id "
        "WHERE l.loan_id = ? AND l.status = 'DISBURSED' AND a.last_tx_id = ? AND changes() > 0;",
        json::array({{{"type", "text"}, {"value", disbursementTransactionId}},
                     {{"type", "text"}, {"value", req.loan_id}},
                     {{"type", "text"}, {"value", disbursementTransactionId}}})
    });
    batch.push_back({
        "INSERT INTO notifications (notification_id, user_id, title, body, notification_type, reference_id) "
        "SELECT ?, user_id, 'Khoản vay đã giải ngân', 'Tiền vay đã được chuyển vào tài khoản thanh toán.', 'LOAN', loan_id "
        "FROM loans WHERE loan_id = ? AND status = 'DISBURSED' AND changes() > 0;",
        json::array({{{"type", "text"}, {"value", notificationId}},
                     {{"type", "text"}, {"value", req.loan_id}}})
    });

    const json batchRes = m_tursoDb.executeBatch(batch);
    bool succeeded = batchRes.contains("results") && batchRes["results"].is_array() && !batchRes["results"].empty();
    if (succeeded) {
        for (const auto& result : batchRes["results"]) {
            if (!result.is_object() || result.value("type", "") != "ok") {
                succeeded = false;
                break;
            }
        }
    }
    if (succeeded) {
        const json statusCheck = m_tursoDb.execute(
            "SELECT COUNT(*), (SELECT COUNT(*) FROM transactions WHERE transaction_id = ? AND type = 'LOAN_DISBURSEMENT') "
            "FROM loan_schedules WHERE loan_id = ?;",
            json::array({{{"type", "text"}, {"value", disbursementTransactionId}},
                         {{"type", "text"}, {"value", req.loan_id}}}));
        try {
            const auto& rows = statusCheck["results"][0]["response"]["result"]["rows"];
            succeeded = !rows.empty() && loanCellInt64(rows[0][0]) == loan.getTermMonths() &&
                        loanCellInt64(rows[0][1]) == 1;
        } catch (...) { succeeded = false; }
    }
    if (succeeded) {
        res.success = true;
        res.message = "Giải ngân tiền vay thành công!";
        res.loan_id = req.loan_id;
        m_auditRepo.logAction(session->user_id, "DISBURSE_LOAN_SUCCESS", req.loan_id,
                              "Disbursed: " + std::to_string(loan.getOriginalAmountVnd()) + " VND");
    } else {
        res.message = "Lỗi CSDL khi thực hiện giải ngân!";
    }
    return res;
}

LoanSchedulesResult LoanService::getSchedules(const std::string& sessionToken, const std::string& loanId) {
    LoanSchedulesResult result;
    const auto session = m_sessionManager.getSession(sessionToken);
    if (!session) { result.message = "Phiên đăng nhập đã hết hạn."; return result; }
    if (loanId.empty()) { result.message = "Thiếu mã khoản vay."; return result; }
    if (session->role != "CUSTOMER" && session->role != "TELLER" && session->role != "ADMIN") {
        result.message = "Bạn không có quyền xem lịch trả nợ."; return result;
    }
    const std::string ownerFilter = session->role == "CUSTOMER" ? " AND l.user_id = ?" : "";
    json args = json::array({{{"type", "text"}, {"value", loanId}}});
    if (session->role == "CUSTOMER") args.push_back({{"type", "text"}, {"value", session->user_id}});
    const json response = m_tursoDb.execute(
        "SELECT s.schedule_id, s.loan_id, s.period_num, s.due_date, s.principal_due_vnd, s.interest_due_vnd, "
        "s.penalty_vnd, s.status FROM loan_schedules s JOIN loans l ON l.loan_id = s.loan_id "
        "WHERE s.loan_id = ?" + ownerFilter + " ORDER BY s.period_num;", args);
    try {
        const auto& rows = response["results"][0]["response"]["result"]["rows"];
        for (const auto& row : rows) {
            LoanScheduleDTO item;
            item.schedule_id = loanCellText(row[0]);
            item.loan_id = loanCellText(row[1]);
            item.period_num = static_cast<int>(loanCellInt64(row[2]));
            item.due_date = loanCellText(row[3]);
            item.principal_due_vnd = loanCellInt64(row[4]);
            item.interest_due_vnd = loanCellInt64(row[5]);
            item.penalty_vnd = loanCellInt64(row[6]);
            item.total_due_vnd = item.principal_due_vnd + item.interest_due_vnd + item.penalty_vnd;
            item.status = loanCellText(row[7]);
            result.schedules.push_back(std::move(item));
        }
        result.success = true;
        result.message = "Đã tải lịch trả nợ.";
    } catch (...) { result.message = "Không đọc được lịch trả nợ từ database."; }
    return result;
}

PayInstallmentResult LoanService::payInstallment(const PayInstallmentRequest& request) {
    PayInstallmentResult result;
    const auto session = m_sessionManager.getSession(request.session_token);
    if (!session || session->role != "CUSTOMER") {
        result.message = "Phiên khách hàng không hợp lệ hoặc đã hết hạn."; return result;
    }
    if (request.schedule_id.empty() || request.account_id.empty()) {
        result.message = "Cần chọn kỳ vay và tài khoản thanh toán."; return result;
    }
    const auto user = m_userRepo.findById(session->user_id);
    if (!user || !user->verifyPin(request.pin)) {
        result.message = "Mã PIN giao dịch không chính xác."; return result;
    }
    const auto accounts = m_accRepo.getAccountsByUserId(session->user_id);
    const auto account = std::find_if(accounts.begin(), accounts.end(), [&](const AccountDTO& item) {
        return item.account_id == request.account_id && item.account_type == "CHECKING" && item.status == "ACTIVE";
    });
    if (account == accounts.end()) { result.message = "Tài khoản thanh toán không hợp lệ."; return result; }

    const json lookup = m_tursoDb.execute(
        "SELECT s.loan_id, s.period_num, s.principal_due_vnd, s.interest_due_vnd, s.penalty_vnd, s.status, l.user_id, l.status, l.term_months "
        "FROM loan_schedules s JOIN loans l ON l.loan_id = s.loan_id WHERE s.schedule_id = ? LIMIT 1;",
        json::array({{{"type", "text"}, {"value", request.schedule_id}}}));
    std::string loanId;
    std::string scheduleStatus;
    int period = 0;
    int termMonths = 0;
    int64_t principal = 0;
    int64_t amount = 0;
    try {
        const auto& rows = lookup["results"][0]["response"]["result"]["rows"];
        if (rows.empty() || loanCellText(rows[0][6]) != session->user_id || loanCellText(rows[0][7]) != "DISBURSED") {
            result.message = "Không tìm thấy kỳ vay còn hiệu lực trong tài khoản của bạn."; return result;
        }
        loanId = loanCellText(rows[0][0]);
        period = static_cast<int>(loanCellInt64(rows[0][1]));
        principal = loanCellInt64(rows[0][2]);
        amount = principal + loanCellInt64(rows[0][3]) + loanCellInt64(rows[0][4]);
        scheduleStatus = loanCellText(rows[0][5]);
        termMonths = static_cast<int>(loanCellInt64(rows[0][8]));
    } catch (...) { result.message = "Không đọc được thông tin kỳ vay."; return result; }
    if (scheduleStatus == "PAID") {
        const std::string description = "Thanh toán kỳ " + std::to_string(period) + "/" + std::to_string(termMonths);
        const json previous = m_tursoDb.execute(
            "SELECT transaction_id, amount_vnd, post_balance_vnd FROM transactions "
            "WHERE receiver_account_id = ? AND type = 'LOAN_REPAYMENT' AND status = 'SUCCESS' AND description = ? "
            "ORDER BY created_at DESC LIMIT 1;",
            json::array({{{"type", "text"}, {"value", "LOAN:" + loanId}},
                         {{"type", "text"}, {"value", description}}}));
        try {
            const auto& row = previous["results"][0]["response"]["result"]["rows"][0];
            result.success = true;
            result.message = "Kỳ vay đã được thanh toán trước đó.";
            result.transaction_id = loanCellText(row[0]);
            result.schedule_id = request.schedule_id;
            result.amount_vnd = loanCellInt64(row[1]);
            result.post_balance_vnd = loanCellInt64(row[2]);
            return result;
        } catch (...) {
            result.message = "Kỳ vay đã thanh toán; không tìm thấy giao dịch để khôi phục kết quả.";
            return result;
        }
    }
    if (amount <= 0 || (scheduleStatus != "UNPAID" && scheduleStatus != "OVERDUE")) {
        result.message = "Kỳ vay này đã thanh toán hoặc không hợp lệ."; return result;
    }

    const std::string transactionId = "TXN_" + AuthSecurity::generateNumericCode(24);
    const std::string notificationId = "NTF_" + AuthSecurity::generateNumericCode(20);
    const std::string restoreStatus = scheduleStatus;
    const json batch = m_tursoDb.executeBatch({
        {"UPDATE loan_schedules SET status = 'PAID', paid_at = ? WHERE schedule_id = ? "
         "AND status IN ('UNPAID','OVERDUE') AND EXISTS (SELECT 1 FROM loans WHERE loan_id = ? AND user_id = ? AND status = 'DISBURSED') "
         "AND NOT EXISTS (SELECT 1 FROM loan_schedules earlier WHERE earlier.loan_id = loan_schedules.loan_id "
         "AND earlier.period_num < loan_schedules.period_num AND earlier.status IN ('UNPAID','OVERDUE')) "
         "AND EXISTS (SELECT 1 FROM accounts WHERE account_id = ? AND user_id = ? AND account_type = 'CHECKING' AND status = 'ACTIVE' AND balance_vnd >= ?);",
         json::array({{{"type", "text"}, {"value", transactionId}}, {{"type", "text"}, {"value", request.schedule_id}},
                      {{"type", "text"}, {"value", loanId}}, {{"type", "text"}, {"value", session->user_id}}, {{"type", "text"}, {"value", request.account_id}},
                      {{"type", "text"}, {"value", session->user_id}}, {{"type", "numeric"}, {"value", amount}}})},
        {"UPDATE accounts SET balance_vnd = balance_vnd - ?, last_tx_id = ? WHERE account_id = ? AND user_id = ? "
         "AND account_type = 'CHECKING' AND status = 'ACTIVE' AND balance_vnd >= ? AND changes() > 0;",
         json::array({{{"type", "numeric"}, {"value", amount}}, {{"type", "text"}, {"value", transactionId}},
                      {{"type", "text"}, {"value", request.account_id}}, {{"type", "text"}, {"value", session->user_id}},
                      {{"type", "numeric"}, {"value", amount}}})},
        {"UPDATE loan_schedules SET status = ?, paid_at = NULL WHERE schedule_id = ? AND status = 'PAID' AND paid_at = ? AND changes() = 0;",
         json::array({{{"type", "text"}, {"value", restoreStatus}}, {{"type", "text"}, {"value", request.schedule_id}},
                      {{"type", "text"}, {"value", transactionId}}})},
        {"INSERT INTO transactions (transaction_id, sender_account_id, receiver_account_id, amount_vnd, fee_vnd, type, status, description, post_balance_vnd) "
         "SELECT ?, ?, 'LOAN:' || ?, ?, 0, 'LOAN_REPAYMENT', 'SUCCESS', ?, balance_vnd FROM accounts "
         "WHERE account_id = ? AND user_id = ? AND last_tx_id = ? AND EXISTS (SELECT 1 FROM loan_schedules WHERE schedule_id = ? AND status = 'PAID');",
         json::array({{{"type", "text"}, {"value", transactionId}}, {{"type", "text"}, {"value", request.account_id}},
                      {{"type", "text"}, {"value", loanId}}, {{"type", "numeric"}, {"value", amount}},
                      {{"type", "text"}, {"value", "Thanh toán kỳ " + std::to_string(period) + "/" + std::to_string(termMonths)}},
                      {{"type", "text"}, {"value", request.account_id}}, {{"type", "text"}, {"value", session->user_id}},
                      {{"type", "text"}, {"value", transactionId}}, {{"type", "text"}, {"value", request.schedule_id}}})},
        {"UPDATE loans SET remaining_debt_vnd = MAX(0, remaining_debt_vnd - ?), "
         "status = CASE WHEN remaining_debt_vnd <= ? THEN 'PAID' ELSE 'DISBURSED' END "
         "WHERE loan_id = ? AND status = 'DISBURSED' AND EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ?) "
         "AND EXISTS (SELECT 1 FROM loan_schedules WHERE schedule_id = ? AND status = 'PAID');",
         json::array({{{"type", "numeric"}, {"value", principal}}, {{"type", "numeric"}, {"value", principal}},
                      {{"type", "text"}, {"value", loanId}}, {{"type", "text"}, {"value", transactionId}},
                      {{"type", "text"}, {"value", request.schedule_id}}})},
        {"UPDATE loan_schedules SET paid_at = CURRENT_TIMESTAMP WHERE schedule_id = ? AND status = 'PAID' AND paid_at = ? "
         "AND EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ? AND status = 'SUCCESS');",
         json::array({{{"type", "text"}, {"value", request.schedule_id}}, {{"type", "text"}, {"value", transactionId}},
                      {{"type", "text"}, {"value", transactionId}}})},
        {"INSERT INTO notifications (notification_id, user_id, title, body, notification_type, reference_id) "
         "SELECT ?, user_id, 'Đã thanh toán kỳ vay', ?, 'LOAN', ? FROM loans WHERE loan_id = ? "
         "AND EXISTS (SELECT 1 FROM transactions WHERE transaction_id = ? AND status = 'SUCCESS');",
         json::array({{{"type", "text"}, {"value", notificationId}},
                      {{"type", "text"}, {"value", "Đã thanh toán kỳ " + std::to_string(period) + "/" + std::to_string(termMonths) +
                          " với số tiền " + std::to_string(amount) + " VND."}},
                      {{"type", "text"}, {"value", loanId}}, {{"type", "text"}, {"value", loanId}},
                      {{"type", "text"}, {"value", transactionId}}})}
    });
    bool batchOk = batch.contains("results") && batch["results"].is_array() && !batch["results"].empty();
    if (batchOk) for (const auto& item : batch["results"]) batchOk = batchOk && item.is_object() && item.value("type", "") == "ok";
    if (!batchOk) { result.message = "Không thể ghi nhận thanh toán kỳ vay."; return result; }

    const json confirmation = m_tursoDb.execute(
        "SELECT amount_vnd, post_balance_vnd FROM transactions WHERE transaction_id = ? AND type = 'LOAN_REPAYMENT' AND status = 'SUCCESS' LIMIT 1;",
        json::array({{{"type", "text"}, {"value", transactionId}}}));
    try {
        const auto& rows = confirmation["results"][0]["response"]["result"]["rows"];
        if (rows.empty()) { result.message = "Chưa ghi nhận thanh toán. Kiểm tra số dư và trạng thái kỳ vay rồi thử lại."; return result; }
        result.amount_vnd = loanCellInt64(rows[0][0]);
        result.post_balance_vnd = loanCellInt64(rows[0][1]);
    } catch (...) { result.message = "Không xác minh được kết quả thanh toán."; return result; }
    result.success = true;
    result.transaction_id = transactionId;
    result.schedule_id = request.schedule_id;
    result.message = "Đã thanh toán kỳ vay.";
    m_auditRepo.logAction(session->user_id, "LOAN_INSTALLMENT_PAID", loanId,
                          "Schedule: " + request.schedule_id + "; amount: " + std::to_string(amount));
    return result;
}

} // namespace Banking
