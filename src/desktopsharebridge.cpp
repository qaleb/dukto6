#include "desktopsharebridge.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSet>
#include <QTimer>
#include <QThread>
#include <QUrl>
#include <memory>

DesktopShareBridge::DesktopShareBridge(const QString &directory, QObject *parent)
    : QObject(parent), mDirectory(QDir(directory).absolutePath()),
      mName("dukto-files-" + QString::fromLatin1(QCryptographicHash::hash(mDirectory.toUtf8(), QCryptographicHash::Sha256).toHex().left(24))),
      mLock(QDir(mDirectory).filePath("instance.lock"))
{
    mLock.setStaleLockTime(0);
    mServer.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&mServer, &QLocalServer::newConnection, this, &DesktopShareBridge::acceptConnections);
}

QStringList DesktopShareBridge::localSources(const QStringList &arguments, const QString &cwd, QString *error)
{
    QStringList result;
    QSet<QString> seen;
    for (const auto &argument : arguments) {
        QString path;
        const QUrl url(argument);
        // Test absolute filesystem paths first, including Windows drive/UNC paths.
        if (QDir::isAbsolutePath(argument)) path = argument;
        else if (url.isLocalFile() && (url.host().isEmpty() || url.host() == "localhost")) path = url.toLocalFile();
        else if (url.scheme().isEmpty()) path = argument;
        else { *error = tr("Only local files and folders can be sent. Download this item first: %1").arg(argument); return {}; }
        if (path.isEmpty() || path.contains(QChar::Null)) { *error = tr("An invalid file location was supplied."); return {}; }
        path = QDir::cleanPath(QDir(cwd).absoluteFilePath(path));
        if (!seen.contains(path)) { result.append(path); seen.insert(path); }
    }
    return result;
}

DesktopShareBridge::Result DesktopShareBridge::start(const QStringList &sources, QString *error)
{
    QJsonArray items;
    for (const auto &source : sources) items.append(source);
    const QByteArray json = QJsonDocument(QJsonObject{{"version", 1}, {"files", items}}).toJson(QJsonDocument::Compact);
    if (sources.size() > 10000 || json.size() > MaxPayload) {
        *error = tr("Too many files were selected. Send a smaller selection."); return Failed;
    }
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream << quint32(json.size()); packet.append(json);
    if (!QDir().mkpath(mDirectory)) { *error = tr("Cannot create Dukto's session directory."); return Failed; }
    if (!mLock.tryLock(0)) {
        if (mLock.error() != QLockFile::LockFailedError) { *error = tr("Cannot access Dukto's session lock."); return Failed; }
        return forward(packet, error) ? Forwarded : Failed;
    }
    // Only the lifetime-lock owner may remove a socket left by a crashed process.
    QLocalServer::removeServer(mName);
    if (!mServer.listen(mName)) { *error = mServer.errorString(); mLock.unlock(); return Failed; }
    if (!sources.isEmpty()) mWaiting.enqueue(sources);
    return Primary;
}

bool DesktopShareBridge::forward(const QByteArray &packet, QString *error)
{
    QElapsedTimer deadline; deadline.start();
    QLocalSocket socket;
    while (deadline.elapsed() < 3000) {
        socket.connectToServer(mName);
        if (socket.waitForConnected(150)) break;
        socket.abort();
        QThread::msleep(25); // Only a secondary launcher waits; the UI process never blocks here.
    }
    if (socket.state() != QLocalSocket::ConnectedState) {
        *error = tr("The running Dukto window is not responding. Close it and try again."); return false;
    }
    if (socket.write(packet) != packet.size()) { *error = socket.errorString(); return false; }
    while (socket.bytesToWrite()) if (!socket.waitForBytesWritten(3000)) { *error = socket.errorString(); return false; }
    QByteArray reply;
    deadline.restart();
    while (!reply.contains('\n') && deadline.elapsed() < 3000) {
        reply += socket.readAll();
        if (!reply.contains('\n') && !socket.waitForReadyRead(qMax(1, 3000 - int(deadline.elapsed())))) break;
    }
    reply += socket.readAll();
    if (reply != "OK\n") { *error = tr("Dukto could not accept the file selection. Try again in the running window."); return false; }
    return true;
}

void DesktopShareBridge::ready()
{
    mReady = true;
    while (!mWaiting.isEmpty()) deliver(mWaiting.dequeue());
}

void DesktopShareBridge::deliver(const QStringList &sources)
{
    if (!sources.isEmpty()) emit filesRequested(sources);
    emit activationRequested();
}

void DesktopShareBridge::acceptConnections()
{
    while (mServer.hasPendingConnections()) {
        auto *socket = mServer.nextPendingConnection();
        if (mClients >= 8) { socket->abort(); socket->deleteLater(); continue; }
        ++mClients;
        socket->setReadBufferSize(MaxPayload + 4);
        auto bytes = std::make_shared<QByteArray>();
        auto *timeout = new QTimer(socket);
        timeout->setSingleShot(true); timeout->start(3000);
        connect(timeout, &QTimer::timeout, socket, &QLocalSocket::abort);
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] { --mClients; socket->deleteLater(); });
        connect(socket, &QLocalSocket::readyRead, this, [this, socket, bytes, timeout] {
            if (socket->property("handled").toBool()) return;
            bytes->append(socket->readAll());
            if (bytes->size() > MaxPayload + 4) { socket->abort(); return; }
            if (bytes->size() < 4) return;
            QDataStream header(bytes->left(4)); quint32 length; header >> length;
            if (!length || length > MaxPayload) { socket->abort(); return; }
            if (bytes->size() < length + 4) return;
            if (bytes->size() != length + 4) { socket->abort(); return; }
            const auto document = QJsonDocument::fromJson(bytes->mid(4));
            const auto object = document.object();
            if (object.value("version").toInt() != 1 || !object.value("files").isArray()) { socket->abort(); return; }
            const auto items = object.value("files").toArray();
            if (items.size() > 10000 || (!mReady && mWaiting.size() >= 20)) { socket->abort(); return; }
            QStringList sources;
            for (const auto &item : items) {
                if (!item.isString() || item.toString().contains(QChar::Null) || !QDir::isAbsolutePath(item.toString())) { socket->abort(); return; }
                sources.append(item.toString());
            }
            timeout->stop();
            socket->setProperty("handled", true);
            if (mReady) deliver(sources); else mWaiting.enqueue(sources);
            socket->write("OK\n"); socket->disconnectFromServer();
        });
    }
}
