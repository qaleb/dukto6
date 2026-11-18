#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLockFile>
#include <QQueue>
#include <QStringList>

// A per-user primary process receives file-manager launches over a local socket.
// The lifetime lock also serializes stale-socket recovery and simultaneous starts.
class DesktopShareBridge : public QObject {
    Q_OBJECT
public:
    enum Result { Primary, Forwarded, Failed };
    explicit DesktopShareBridge(const QString &scopeDirectory, QObject *parent = nullptr);
    Result start(const QStringList &sources, QString *error);
    void ready();
    QString serverName() const { return mName; }
    static QStringList localSources(const QStringList &arguments, const QString &workingDirectory, QString *error);
    static constexpr int MaxPayload = 1024 * 1024;
signals:
    void filesRequested(QStringList sources);
    void activationRequested();
private:
    bool forward(const QByteArray &packet, QString *error);
    void acceptConnections();
    void deliver(const QStringList &sources);
    QString mDirectory, mName;
    QLockFile mLock;
    QLocalServer mServer;
    QQueue<QStringList> mWaiting;
    int mClients = 0;
    bool mReady = false;
};
