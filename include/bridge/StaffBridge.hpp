#ifndef STAFF_BRIDGE_HPP
#define STAFF_BRIDGE_HPP

#include <QObject>
#include <QString>

namespace Banking {

class StaffService;

class StaffBridge : public QObject {
    Q_OBJECT

private:
    StaffService& m_service;

public:
    explicit StaffBridge(StaffService& service, QObject* parent = nullptr);

    Q_INVOKABLE QString getSystemConfig(const QString& jsonRequest);
    Q_INVOKABLE QString updateSystemConfig(const QString& jsonRequest);
    Q_INVOKABLE QString getStaffMembers(const QString& jsonRequest);
    Q_INVOKABLE QString createStaff(const QString& jsonRequest);
    Q_INVOKABLE QString getDashboardStats(const QString& jsonRequest);
};

} // namespace Banking

#endif // STAFF_BRIDGE_HPP
