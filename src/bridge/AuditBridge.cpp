#include "bridge/AuditBridge.hpp"
#include "services/AuditService.hpp"
#include "dtos/AuditDTOs.hpp"
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

AuditBridge::AuditBridge(AuditService& service, QObject* parent)
    : QObject(parent), m_service(service) {}

QString AuditBridge::getAuditLogs(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<GetAuditLogsRequest>();
        GetAuditLogsResult res = m_service.getLogs(req);
        json j = res;
        return QString::fromStdString(j.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Định dạng JSON yêu cầu không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

QString AuditBridge::getAuditLogById(const QString& jsonRequest) {
    try {
        const auto req = json::parse(jsonRequest.toStdString());
        const auto result = m_service.getLogById(req.value("session_token", ""), req.value("log_id", ""));
        return QString::fromStdString(json(result).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu tra cứu nhật ký không hợp lệ."}}).dump());
    }
}

} // namespace Banking
