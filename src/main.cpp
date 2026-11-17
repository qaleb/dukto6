#include <QGuiApplication>
#if !defined(Q_OS_ANDROID)
#include <QApplication>
#endif
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QFile>
#include <QStringList>

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

    QQmlApplicationEngine engine;

    // qDebug() << "FileUtils exists?" << QFile::exists(":/src/libs/FileUtils.java");

    // Use the singleton instance of GuiBehind
    GuiBehind::instance(engine);

    engine.loadFromModule("dukto6", "Main");

    return app.exec();
}
