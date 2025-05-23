#include <QApplication> // Add this include
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QFile>

#include "guibehind.h"

int main(int argc, char *argv[])
{
#if defined(Q_OS_WIN) || defined(Q_OS_UNIX)
    QApplication app(argc, argv); // Use QApplication for desktop
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
