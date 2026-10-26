#ifndef PAGE_BRIDGE_HPP
#define PAGE_BRIDGE_HPP

#include <QObject>
#include <QString>
#include "services/PageDataLoaderService.hpp"

namespace Banking {

class PageBridge : public QObject {
    Q_OBJECT

private:
    PageDataLoaderService& m_pageDataLoader;

public:
    explicit PageBridge(PageDataLoaderService& pageDataLoader, QObject* parent = nullptr);

    /**
     * @brief Slot để JS Frontend gọi trực tiếp qua QWebChannel
     * @param jsonRequest Chuỗi JSON dạng: { "page": "admin:dashboard", "session_token": "..." }
     * @return Chuỗi JSON đóng gói DTO phản hồi trả về UI (dùng cho gọi đồng bộ / Promise)
     */
    Q_INVOKABLE QString getPageData(const QString& jsonRequest);

    /**
     * @brief Slot nạp dữ liệu bất đồng bộ (Non-blocking GUI)
     * Kết quả sẽ được trả về thông qua signal pageDataReady
     */
    Q_INVOKABLE void getPageDataAsync(const QString& jsonRequest);

signals:
    /**
     * @brief Signal bắn kết quả về cho JS khi xử lý bất đồng bộ hoàn tất
     */
    void pageDataReady(const QString& jsonResponse);
};

} // namespace Banking

#endif // PAGE_BRIDGE_HPP