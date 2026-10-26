#include "bridge/SavingsBridge.hpp"
#include "services/SavingsService.hpp"
#include "dtos/SavingsDTOs.hpp"
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

SavingsBridge::SavingsBridge(SavingsService& savingsService, QObject* parent)
    : QObject(parent), m_savingsService(savingsService) {}

// 1. API Mở sổ tiết kiệm
QString SavingsBridge::openSavings(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<OpenSavingsRequest>();
        SavingsOperationResult res = m_savingsService.openSavings(req);
        json resJson = res;
        return QString::fromStdString(resJson.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Dữ liệu Mở sổ Tiết kiệm không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

// 2. API Tất toán sổ tiết kiệm
QString SavingsBridge::closeSavings(const QString& jsonRequest) {
    try {
        auto req = json::parse(jsonRequest.toStdString()).get<CloseSavingsRequest>();
        SavingsOperationResult res = m_savingsService.closeSavings(req);
        json resJson = res;
        return QString::fromStdString(resJson.dump());
    } catch (...) {
        json err = {{"success", false}, {"message", "Dữ liệu Tất toán không hợp lệ!"}};
        return QString::fromStdString(err.dump());
    }
}

} // namespace Banking