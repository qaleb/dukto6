#include <QtTest>
#include <QCoreApplication>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QTemporaryDir>
#include "desktopsharebridge.h"

class DesktopShareTest : public QObject {
    Q_OBJECT
private slots:
    void relativePathsAndUrlsAreCapturedBeforeWorkingDirectoryChanges() {
        QString error;
        const auto files = DesktopShareBridge::localSources({"a b.txt", "-leading.txt", "file:///tmp/r%C3%A9sum%C3%A9.txt", "a b.txt"}, "/tmp/selection", &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(files, QStringList({"/tmp/selection/a b.txt", "/tmp/selection/-leading.txt", QString::fromUtf8("/tmp/résumé.txt")}));
        QVERIFY(DesktopShareBridge::localSources({"https://example.test/file"}, "/tmp", &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
    void crossProcessHandoffAndActivation() {
        QTemporaryDir scope;
        DesktopShareBridge primary(scope.path());
        QString error;
        QCOMPARE(primary.start({"/tmp/first.txt"}, &error), DesktopShareBridge::Primary);
        QSignalSpy files(&primary, &DesktopShareBridge::filesRequested), activated(&primary, &DesktopShareBridge::activationRequested);
        QCOMPARE(files.count(), 0);
        primary.ready(); QCOMPARE(files.count(), 1);
        const QString unicode = QString::fromUtf8("/tmp/résumé-文件.txt");
        QProcess second;
        second.start(QCoreApplication::applicationFilePath(), {"--forward", scope.path(), "/tmp/has spaces.txt", unicode});
        QTRY_COMPARE(second.state(), QProcess::NotRunning);
        QCOMPARE(second.exitCode(), 0);
        QCOMPARE(files.count(), 2);
        QCOMPARE(files[1][0].toStringList(), QStringList({"/tmp/has spaces.txt", unicode}));
        QProcess activate;
        activate.start(QCoreApplication::applicationFilePath(), {"--forward", scope.path()});
        QTRY_COMPARE(activate.state(), QProcess::NotRunning);
        QCOMPARE(activate.exitCode(), 0);
        QCOMPARE(files.count(), 2); QCOMPARE(activated.count(), 3);
    }
    void fragmentedPacketsAndInvalidMessages() {
        QTemporaryDir scope;
        DesktopShareBridge primary(scope.path()); QString error;
        QCOMPARE(primary.start({}, &error), DesktopShareBridge::Primary); primary.ready();
        QSignalSpy files(&primary, &DesktopShareBridge::filesRequested);
        QLocalSocket sender; sender.connectToServer(primary.serverName());
        QVERIFY(sender.waitForConnected());
        const QByteArray json = QJsonDocument(QJsonObject{{"version", 1}, {"files", QJsonArray{"/tmp/line\nbreak.txt"}}}).toJson(QJsonDocument::Compact);
        QByteArray packet; QDataStream stream(&packet, QIODevice::WriteOnly); stream << quint32(json.size()); packet += json;
        for (int i = 0; i < packet.size(); ++i) { sender.write(packet.mid(i, 1)); sender.flush(); QTest::qWait(1); }
        QTRY_COMPARE(files.count(), 1);
        QCOMPARE(files[0][0].toStringList(), QStringList({"/tmp/line\nbreak.txt"}));
        QLocalSocket bad; bad.connectToServer(primary.serverName()); QVERIFY(bad.waitForConnected());
        QByteArray excessive; QDataStream header(&excessive, QIODevice::WriteOnly); header << quint32(DesktopShareBridge::MaxPayload + 1);
        bad.write(excessive); bad.flush();
        QTRY_COMPARE(bad.state(), QLocalSocket::UnconnectedState);
        QCOMPARE(files.count(), 1);
    }
    void killedPrimaryRecoversItsLockAndSocket() {
        QTemporaryDir scope;
        QProcess old;
        old.start(QCoreApplication::applicationFilePath(), {"--candidate", scope.path()});
        QTRY_VERIFY(old.bytesAvailable()); QCOMPARE(old.readAllStandardOutput().trimmed(), QByteArray("PRIMARY"));
        old.kill(); QTRY_COMPARE(old.state(), QProcess::NotRunning);
        DesktopShareBridge replacement(scope.path()); QString error;
        QCOMPARE(replacement.start({}, &error), DesktopShareBridge::Primary);
    }
    void concurrentLaunchesHaveOnePrimary() {
        QTemporaryDir scope;
        QProcess candidates[4];
        for (auto &process : candidates) process.start(QCoreApplication::applicationFilePath(), {"--candidate", scope.path()});
        int primaries = 0;
        for (auto &process : candidates) {
            QTRY_VERIFY(process.bytesAvailable());
            const auto result = process.readAllStandardOutput().trimmed();
            if (result == "PRIMARY") ++primaries;
            else QCOMPARE(result, QByteArray("FORWARDED"));
        }
        QCOMPARE(primaries, 1);
        for (auto &process : candidates) { if (process.state() != QProcess::NotRunning) process.kill(); process.waitForFinished(); }
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() >= 3 && (args[1] == "--forward" || args[1] == "--candidate")) {
        DesktopShareBridge bridge(args[2]); QString error;
        const auto result = bridge.start(args.mid(3), &error);
        if (args[1] == "--forward") return result == DesktopShareBridge::Forwarded ? 0 : 1;
        if (result == DesktopShareBridge::Primary) { puts("PRIMARY"); fflush(stdout); bridge.ready(); return app.exec(); }
        if (result == DesktopShareBridge::Forwarded) { puts("FORWARDED"); return 0; }
        fprintf(stderr, "%s\n", qPrintable(error)); return 1;
    }
    DesktopShareTest test; return QTest::qExec(&test, argc, argv);
}
#include "desktopshare_tests.moc"
