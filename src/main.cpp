#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#include <QWebChannel>
#include <QWebEngineView>

#include <core/EnvLoader.hpp>
#include <core/SessionManager.hpp>
#include <core/TursoDB.hpp>
#include <bridge/AuthBridge.hpp>
#include <bridge/AccountBridge.hpp>
#include <bridge/AuditBridge.hpp>
#include <bridge/BillBridge.hpp>
#include <bridge/LoanBridge.hpp>
#include <bridge/PageBridge.hpp>
#include <bridge/SavingsBridge.hpp>
#include <bridge/TransactionBridge.hpp>
#include <bridge/UserBridge.hpp>
#include <bridge/StaffBridge.hpp>
#include <bridge/NotificationBridge.hpp>
#include <bridge/CreditBridge.hpp>
#include <repository/AccountRepository.hpp>
#include <repository/AuditRepository.hpp>
#include <repository/AuthRepository.hpp>
#include <repository/BillRepository.hpp>
#include <repository/LoansRepository.hpp>
#include <repository/SavingsRepository.hpp>
#include <repository/TransactionRepository.hpp>
#include <repository/UserRepository.hpp>
#include <repository/NotificationRepository.hpp>
#include <services/AccountService.hpp>
#include <services/AuditService.hpp>
#include <services/AuthService.hpp>
#include <services/BillService.hpp>
#include <services/LoansService.hpp>
#include <services/PageDataLoaderService.hpp>
#include <services/SavingsService.hpp>
#include <services/StaffService.hpp>
#include <services/TransactionService.hpp>
#include <services/UserService.hpp>
#include <services/NotificationService.hpp>
#include <services/CreditService.hpp>

#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#ifndef OOPBANK_PROJECT_DIR
#define OOPBANK_PROJECT_DIR "."
#endif

#ifndef OOPBANK_UI_DIR
#define OOPBANK_UI_DIR "."
#endif

namespace {

struct CustomerBackend {
    Banking::TursoDB database;
    Banking::SessionManager sessions;
    Banking::NotificationRepository notificationRepository;
    Banking::AuthRepository authRepository;
    Banking::UserRepository userRepository;
    Banking::AccountRepository accountRepository;
    Banking::AuditRepository auditRepository;
    Banking::BillRepository billRepository;
    Banking::LoanRepository loanRepository;
    Banking::SavingsRepository savingsRepository;
    Banking::TransactionRepository transactionRepository;
    Banking::AuthService authService;
    Banking::UserService userService;
    Banking::AccountService accountService;
    Banking::AuditService auditService;
    Banking::BillService billService;
    Banking::LoanService loanService;
    Banking::SavingsService savingsService;
    Banking::TransactionService transactionService;
    Banking::NotificationService notificationService;
    Banking::CreditService creditService;
    Banking::PageDataLoaderService pageDataLoader;
    Banking::StaffService staffService;
    Banking::AuthBridge authBridge;
    Banking::AccountBridge accountBridge;
    Banking::AuditBridge auditBridge;
    Banking::BillBridge billBridge;
    Banking::LoanBridge loanBridge;
    Banking::PageBridge pageBridge;
    Banking::SavingsBridge savingsBridge;
    Banking::TransactionBridge transactionBridge;
    Banking::UserBridge userBridge;
    Banking::StaffBridge staffBridge;
    Banking::NotificationBridge notificationBridge;
    Banking::CreditBridge creditBridge;

    CustomerBackend(std::string databaseUrl, std::string authToken)
        : database(std::move(databaseUrl), std::move(authToken)),
          notificationRepository(database),
          authRepository(database),
          userRepository(database),
          accountRepository(database),
          auditRepository(database),
          billRepository(database),
          loanRepository(database),
          savingsRepository(database),
          transactionRepository(database),
          authService(authRepository, userRepository, auditRepository, sessions),
          userService(userRepository, auditRepository, sessions),
          accountService(accountRepository, userRepository, auditRepository),
          auditService(auditRepository, userRepository, sessions),
          billService(billRepository, userRepository, accountRepository, transactionRepository, auditRepository, sessions),
          loanService(loanRepository, accountRepository, userRepository, auditRepository, sessions, database),
          savingsService(savingsRepository, accountRepository, userRepository, auditRepository, sessions, database),
          transactionService(transactionRepository, userRepository, accountRepository, billRepository, auditRepository, sessions, database),
          notificationService(notificationRepository, sessions),
          creditService(database, userRepository, sessions),
          pageDataLoader(userService, accountService, loanService, savingsService, notificationService),
          staffService(database, userRepository, auditRepository, sessions),
          authBridge(authService),
          accountBridge(accountService, sessions),
          auditBridge(auditService),
          billBridge(billService),
          loanBridge(loanService),
          pageBridge(pageDataLoader),
          savingsBridge(savingsService),
          transactionBridge(transactionService),
          userBridge(userService),
          staffBridge(staffService),
          notificationBridge(notificationService),
          creditBridge(creditService) {
        if (!notificationRepository.prepareTransactionNotifications()) {
            std::cerr << "[Notification] Could not prepare transaction-backed notifications.\n";
        }
    }
};

std::string firstConfiguredValue(std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const std::string value = Banking::EnvLoader::get(key);
        if (!value.empty()) return value;
    }
    return {};
}

}  // namespace

int main(int argc, char* argv[]) {  
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("OOPBank"));

    const QString configuredProjectDirectory = QString::fromUtf8(OOPBANK_PROJECT_DIR);
    const QString appDirectory = QCoreApplication::applicationDirPath();
    QString projectDirectory = configuredProjectDirectory;
    const QStringList projectCandidates = {
        configuredProjectDirectory,
        appDirectory,
        QDir(appDirectory).filePath(QStringLiteral("..")),
        QDir(appDirectory).filePath(QStringLiteral("../..")),
    };
    for (const QString& candidate : projectCandidates) {
        const QString normalized = QDir(candidate).absolutePath();
        const QString candidateIndex = QDir(normalized).filePath(QStringLiteral("app/index.html"));
        if (QFileInfo::exists(candidateIndex)) {
            projectDirectory = normalized;
            break;
        }
    }
    const QStringList envCandidates = {
        QDir(appDirectory).filePath(QStringLiteral(".env")),
        QDir(projectDirectory).filePath(QStringLiteral(".env")),
        QDir(appDirectory).filePath(QStringLiteral("../.env")),
    };
    for (const QString& envPath : envCandidates) {
        if (QFileInfo::exists(envPath)) { Banking::EnvLoader::load(envPath.toStdString()); break; }
    }

    const std::string databaseUrl = firstConfiguredValue({"TURSO_URL", "TURSO_DATABASE_URL"});
    const std::string authToken = firstConfiguredValue({"TURSO_TOKEN", "TURSO_AUTH_TOKEN"});

    // Declare the backend before Qt objects so it stays alive until after the page/channel close.
    std::unique_ptr<CustomerBackend> backend;
    QWebChannel webChannel;
    QWebEngineView view;
    view.setWindowTitle(QStringLiteral("OOPBank"));
    view.resize(1280, 820);
    view.page()->setWebChannel(&webChannel);

    if (!databaseUrl.empty() && !authToken.empty()) {
        backend = std::make_unique<CustomerBackend>(databaseUrl, authToken);
        webChannel.registerObject(QStringLiteral("authBridge"), &backend->authBridge);
        webChannel.registerObject(QStringLiteral("accountBridge"), &backend->accountBridge);
        webChannel.registerObject(QStringLiteral("auditBridge"), &backend->auditBridge);
        webChannel.registerObject(QStringLiteral("billBridge"), &backend->billBridge);
        webChannel.registerObject(QStringLiteral("loanBridge"), &backend->loanBridge);
        webChannel.registerObject(QStringLiteral("pageBridge"), &backend->pageBridge);
        webChannel.registerObject(QStringLiteral("savingsBridge"), &backend->savingsBridge);
        webChannel.registerObject(QStringLiteral("transactionBridge"), &backend->transactionBridge);
        webChannel.registerObject(QStringLiteral("userBridge"), &backend->userBridge);
        webChannel.registerObject(QStringLiteral("staffBridge"), &backend->staffBridge);
        webChannel.registerObject(QStringLiteral("notificationBridge"), &backend->notificationBridge);
        webChannel.registerObject(QStringLiteral("creditBridge"), &backend->creditBridge);
        std::string connectionError;
        if (!backend->database.checkConnection(connectionError)) {
            QMessageBox::warning(
                nullptr,
                QStringLiteral("Không kết nối được Turso"),
                QString::fromStdString(connectionError) +
                    QStringLiteral("\nỨng dụng sẽ không dùng dữ liệu mẫu; hãy kiểm tra URL, token và kết nối mạng."));
        }
    } else {
        QMessageBox::warning(
            nullptr,
            QStringLiteral("Thiếu cấu hình database"),
            QStringLiteral("Chưa có TURSO_URL/TURSO_TOKEN (hoặc TURSO_DATABASE_URL/TURSO_AUTH_TOKEN) trong .env hay biến môi trường. Giao diện sẽ mở bằng dữ liệu mẫu."));
    }

    const QStringList uiCandidates = {
        QString::fromUtf8(OOPBANK_UI_DIR),
        QDir(appDirectory).filePath(QStringLiteral("docs")),
        QDir(appDirectory).filePath(QStringLiteral("resources/docs")),
    };
    QString uiIndex;
    for (const QString& uiDirectory : uiCandidates) {
        const QString candidate = QDir(uiDirectory).filePath(QStringLiteral("app/index.html"));
        if (QFileInfo::exists(candidate)) { uiIndex = candidate; break; }
    }
    if (uiIndex.isEmpty()) uiIndex = QDir(QString::fromUtf8(OOPBANK_UI_DIR)).filePath(QStringLiteral("app/index.html"));
    if (!QFileInfo::exists(uiIndex)) {
        QMessageBox::critical(nullptr, QStringLiteral("Không tìm thấy giao diện"), uiIndex);
        return 1;
    }

    view.load(QUrl::fromLocalFile(uiIndex));
    view.show();
    return app.exec();
}
