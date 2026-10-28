#include <bridge/StaffBridge.hpp>

#include <core/json.hpp>
#include <dtos/StaffDTOs.hpp>
#include <services/StaffService.hpp>

namespace Banking {

using json = nlohmann::json;

StaffBridge::StaffBridge(StaffService& service, QObject* parent)
    : QObject(parent), m_service(service) {}

QString StaffBridge::getDashboardStats(const QString& jsonRequest) {
    try {
        const auto request = json::parse(jsonRequest.toStdString()).get<GetSystemConfigRequest>();
        return QString::fromStdString(json(m_service.getDashboardStats(request.session_token)).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu thống kê không hợp lệ."}}).dump());
    }
}

QString StaffBridge::getSystemConfig(const QString& jsonRequest) {
    try {
        const auto request = json::parse(jsonRequest.toStdString()).get<GetSystemConfigRequest>();
        return QString::fromStdString(json(m_service.getSystemConfig(request)).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu cấu hình không hợp lệ."}}).dump());
    }
}

QString StaffBridge::updateSystemConfig(const QString& jsonRequest) {
    try {
        const auto request = json::parse(jsonRequest.toStdString()).get<UpdateSystemConfigRequest>();
        return QString::fromStdString(json(m_service.updateSystemConfig(request)).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu cấu hình không hợp lệ."}}).dump());
    }
}

QString StaffBridge::getStaffMembers(const QString& jsonRequest) {
    try {
        const auto request = json::parse(jsonRequest.toStdString()).get<GetSystemConfigRequest>();
        return QString::fromStdString(json(m_service.getStaffMembers(request.session_token)).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu danh sách nhân viên không hợp lệ."}}).dump());
    }
}

QString StaffBridge::createStaff(const QString& jsonRequest) {
    try {
        const auto request = json::parse(jsonRequest.toStdString()).get<CreateStaffRequest>();
        return QString::fromStdString(json(m_service.createStaff(request)).dump());
    } catch (...) {
        return QString::fromStdString(json({{"success", false}, {"message", "Yêu cầu tạo nhân viên không hợp lệ."}}).dump());
    }
}

} // namespace Banking
