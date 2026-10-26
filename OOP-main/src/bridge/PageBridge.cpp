#include "bridge/PageBridge.hpp"
#include <QtConcurrent/QtConcurrent>
#include <core/json.hpp>

namespace Banking {

using json = nlohmann::json;

PageBridge::PageBridge(PageDataLoaderService& pageDataLoader, QObject* parent)
    : QObject(parent), m_pageDataLoader(pageDataLoader) {}

QString PageBridge::getPageData(const QString& jsonRequest) {
    try {
        // 1. Giải mã Request JSON từ UI
        json jReq = json::parse(jsonRequest.toStdString());
        std::string pageName = jReq.value("page", "");
        std::string sessionToken = jReq.value("session_token", "");

        // 2. Gọi PageDataLoaderService lấy nlohmann::json DTO
        json jRes = m_pageDataLoader.loadDataForPage(pageName, sessionToken);

        // 3. Đóng gói trả về QString cho UI
        return QString::fromStdString(jRes.dump());

    } catch (const std::exception& e) {
        json err = {
            {"success", false},
            {"page", ""},
            {"message", std::string("Lỗi định dạng Request: ") + e.what()}
        };
        return QString::fromStdString(err.dump());
    }
}

void PageBridge::getPageDataAsync(const QString& jsonRequest) {
    // Đẩy tác vụ nạp DB xuống Worker Thread bằng QtConcurrent để không gây treo UI
    QtConcurrent::run([this, jsonRequest]() {
        QString responseStr = this->getPageData(jsonRequest);
        
        // Bắn Signal thông báo cho JS sau khi nạp xong
        emit pageDataReady(responseStr);
    });
}

} // namespace Banking