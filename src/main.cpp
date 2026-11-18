#include <QGuiApplication>
#if !defined(Q_OS_ANDROID)
#include <QApplication>
#endif
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QFile>
#include <QStringList>
#include <QTimer>
#include <QCommandLineParser>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QWindow>
#if !defined(Q_OS_ANDROID)
#include <QMessageBox>
#include <QTcpServer>
#include "desktopsharebridge.h"
#include "desktopintegration.h"
#endif

#include "guibehind.h"

int main(int argc, char *argv[])
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Qt 6.9's Adwaita Wayland decoration ignores the title-bar button hints.
    // Prefer XWayland on GNOME so our fixed-size window can omit maximize.
    // Keep explicit platform choices and native Wayland-only sessions working.
    if (qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland"
        && qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':').contains("GNOME", Qt::CaseInsensitive)
        && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")
        && !qEnvironmentVariableIsEmpty("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "xcb;wayland");
    }
#endif

#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID))
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif

    QCoreApplication::setApplicationName("dukto");
    QCoreApplication::setOrganizationName("idv.coolshou");
    QCoreApplication::setOrganizationDomain("com.dukto");
    QCoreApplication::setApplicationVersion(APP_VERSION); // Set version from CMake

    QIcon icon(":/src/assets/dukto.png"); // Set the app icon
    app.setWindowIcon(icon);

#if !defined(Q_OS_ANDROID)
    QCommandLineParser parser;
    parser.setApplicationDescription("Send files and text to another device on your LAN.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"share", "Choose a recipient for the selected files or folders."});
    parser.addOption({"install-file-actions", "Install or repair file-manager actions for this user."});
    parser.addOption({"remove-file-actions", "Remove this user's file-manager actions."});
    parser.addPositionalArgument("files", "Local paths or file:// URLs to send.", "[files...]");
    parser.process(app);
    if (parser.isSet("install-file-actions") || parser.isSet("remove-file-actions")) {
        QString error;
        const bool installing = parser.isSet("install-file-actions");
        const bool okay = installing ? DesktopIntegration::install(app.applicationFilePath(), &error) : DesktopIntegration::remove(&error);
        if (okay) {
            QSettings settings("dukto", "Dukto");
            settings.setValue("Desktop/FileActionsEnabled", installing);
            settings.setValue("Desktop/FileActionsExecutable", installing ? app.applicationFilePath() : QString());
        } else qCritical().noquote() << error;
        return okay ? 0 : 1;
    }
    QString error;
    // Capture relative paths before the backend switches to the receive folder.
    const auto sources = DesktopShareBridge::localSources(parser.positionalArguments(), QDir::currentPath(), &error);
    if (!error.isEmpty()) { QMessageBox::warning(nullptr, "Dukto", error); return 1; }
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) runtime = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    DesktopShareBridge desktop(QDir(runtime).filePath("dukto-session"));
    const auto result = desktop.start(sources, &error);
    if (result == DesktopShareBridge::Forwarded) return 0;
    if (result == DesktopShareBridge::Failed) { QMessageBox::warning(nullptr, "Dukto", error); return 1; }
    // Old Dukto versions do not participate in the session lock.
    QTcpServer probe;
    if (!probe.listen(QHostAddress::Any, 4644)) {
        QMessageBox::warning(nullptr, "Dukto", QObject::tr("Another app is using Dukto's receive port. Close other Dukto windows and try again."));
        return 1;
    }
    probe.close();
    app.setDesktopFileName("dukto");
#endif

    QQmlApplicationEngine engine;

    // qDebug() << "FileUtils exists?" << QFile::exists(":/src/libs/FileUtils.java");

    // Use the singleton instance of GuiBehind
    auto &backend = GuiBehind::instance(engine);

#if !defined(Q_OS_ANDROID)
    QObject::connect(&desktop, &DesktopShareBridge::filesRequested, &backend, [&backend](const QStringList &files) { backend.stageShare(files, {}); });
    QObject::connect(&desktop, &DesktopShareBridge::activationRequested, &engine, [&engine] {
        if (engine.rootObjects().isEmpty()) return;
        auto *window = qobject_cast<QWindow *>(engine.rootObjects().first());
        if (!window) return;
        if (window->visibility() == QWindow::Minimized) window->showNormal();
        window->show(); window->raise(); window->requestActivate();
    });
#endif

    engine.loadFromModule("dukto6", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    QTimer::singleShot(0, &backend, &GuiBehind::readyForShares);
#if !defined(Q_OS_ANDROID)
    QTimer::singleShot(0, &desktop, [&desktop] { desktop.ready(); });
    QTimer::singleShot(0, &app, [&app] { DesktopIntegration::ensureInstalled(app.applicationFilePath()); });
#endif

    return app.exec();
}
