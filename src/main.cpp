#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QFile>

#include "guibehind.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setApplicationName("dukto");
    QCoreApplication::setOrganizationName("idv.coolshou");
    QCoreApplication::setOrganizationDomain("com.dukto");

    QIcon icon(":/src/assets/dukto.png"); // Set the app icon
    app.setWindowIcon(icon);

    QQmlApplicationEngine engine;

    qDebug() << "FileUtils exists?" << QFile::exists(":/src/libs/FileUtils.java");

    // Use the singleton instance of GuiBehind
    GuiBehind::instance(engine);

    engine.loadFromModule("dukto6", "Main");

    return app.exec();
}
