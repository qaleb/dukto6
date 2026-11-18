#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include "desktopintegration.h"

class DesktopIntegrationTest : public QObject {
    Q_OBJECT
private slots:
    void shellLaunchPreservesArgumentsAndCustomFiles() {
#if !defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
        QSKIP("This exercises Linux file-manager scripts without changing native user shortcuts.");
#endif
        QTemporaryDir directory;
        const QByteArray previous = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", directory.path().toUtf8());
        struct Restore { QByteArray old; ~Restore() { if (old.isNull()) qunsetenv("XDG_DATA_HOME"); else qputenv("XDG_DATA_HOME", old); } } restore{previous};
        const QString executable = directory.filePath("Dukto's $special `name` %f");
        QFile fake(executable); QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/usr/bin/python3\nimport os,json,sys\nopen(os.environ['DUKTO_TEST_CAPTURE'],'w').write(json.dumps(sys.argv[1:]))\n"); fake.close();
        QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const QString output = directory.filePath("capture.json");
        QString error; QVERIFY2(DesktopIntegration::install(executable, &error), qPrintable(error));
        const auto script = directory.filePath("nautilus/scripts/Send with Dukto");
        const auto service = directory.filePath("kio/servicemenus/dukto-send.desktop");
        QVERIFY(QFileInfo(service).isExecutable());
        QVERIFY(QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        QVERIFY(DesktopIntegration::install(executable, &error));
        QVERIFY(QFileInfo(script).isExecutable());
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("DUKTO_TEST_CAPTURE", output); process.setProcessEnvironment(environment);
        const QStringList selection = {"/tmp/a b.txt", "/tmp/$(not-a-command).txt", "/tmp/line\nbreak.txt", "/tmp/-leading.txt"};
        process.start(script, selection); QVERIFY(process.waitForFinished()); QCOMPARE(process.exitCode(), 0);
        QFile capture(output); QVERIFY(capture.open(QIODevice::ReadOnly));
        QJsonArray expected; expected.append("--share"); expected.append("--"); for (const auto &path : selection) expected.append(path);
        QCOMPARE(QJsonDocument::fromJson(capture.readAll()).array(), expected); capture.close();
        QVERIFY(DesktopIntegration::remove(&error)); QVERIFY(!QFile::exists(script));
        QFile custom(script); QVERIFY(custom.open(QIODevice::WriteOnly)); custom.write("#!/bin/sh\n# My own custom file\n"); custom.close();
        QVERIFY(!DesktopIntegration::install(executable, &error));
        QVERIFY(DesktopIntegration::remove(&error)); QVERIFY(QFile::exists(script));
    }
    void desktopExecEscapingSurvivesGio() {
#ifndef Q_OS_LINUX
        QSKIP("Uses the freedesktop launcher implementation.");
#endif
        if (QStandardPaths::findExecutable("gio").isEmpty()) QSKIP("gio is not installed");
        QTemporaryDir directory;
        const QString executable = directory.filePath("quote\" dollar$ tick` percent% slash\\ app");
        QFile fake(executable); QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/usr/bin/python3\nimport os,json,sys\nopen(os.environ['DUKTO_TEST_CAPTURE'],'w').write(json.dumps(sys.argv[1:]))\n"); fake.close();
        QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        QFile desktop(directory.filePath("test.desktop")); QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write("[Desktop Entry]\nType=Application\nName=Dukto test\nExec=/usr/bin/env -- " + DesktopIntegration::desktopQuote(executable).toUtf8() + " --share -- %U\n"); desktop.close();
        const QString output = directory.filePath("capture.json");
        QProcess launch;
        auto env = QProcessEnvironment::systemEnvironment(); env.insert("DUKTO_TEST_CAPTURE", output); launch.setProcessEnvironment(env);
        launch.start("gio", {"launch", desktop.fileName(), "/tmp/a b.txt"}); QVERIFY(launch.waitForFinished());
        QVERIFY2(launch.exitCode() == 0, launch.readAllStandardError().constData());
        QTRY_VERIFY(QFile::exists(output));
        QFile capture(output); QVERIFY(capture.open(QIODevice::ReadOnly));
        const auto args = QJsonDocument::fromJson(capture.readAll()).array();
        QCOMPARE(args.size(), 3); QCOMPARE(args[0].toString(), QString("--share"));
        const auto value = args[2].toString(); const QUrl url(value);
        // %U implementations may pass local files as paths or file:// URLs.
        QCOMPARE(url.isLocalFile() ? url.toLocalFile() : value, QString("/tmp/a b.txt"));
    }
};
QTEST_GUILESS_MAIN(DesktopIntegrationTest)
#include "desktopintegration_tests.moc"
