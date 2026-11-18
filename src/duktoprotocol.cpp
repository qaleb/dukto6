#include "duktoprotocol.h"
#include "platform.h"
#include <QDir>
#include <QFileInfo>
#include <QNetworkInterface>
#include <QStorageInfo>
#include <QDateTime>
#include <QSet>
#include <QRegularExpression>
#include <cstring>
#include <limits>

namespace {
constexpr qint64 Chunk = 64 * 1024;
constexpr qint64 Backlog = 256 * 1024;
constexpr qint64 MaxText = 16 * 1024 * 1024;
constexpr qint64 MaxElements = 1000000;
const QString TextName = QStringLiteral("___DUKTO___TEXT___");
void appendNumber(QByteArray &bytes, qint64 number) { bytes.append(reinterpret_cast<const char *>(&number), sizeof(number)); }
qint64 takeNumber(QTcpSocket *socket) { qint64 number = 0; socket->read(reinterpret_cast<char *>(&number), sizeof(number)); return number; }
}

DuktoProtocol::DuktoProtocol(QObject *parent) : QObject(parent) {}
DuktoProtocol::~DuktoProtocol() { resetSocket(); if (mPendingIncomingSocket) delete mPendingIncomingSocket; }
void DuktoProtocol::setPorts(qint16 udp, qint16 tcp) { mUdpPort = quint16(udp); mTcpPort = quint16(tcp); }
void DuktoProtocol::initialize() {
    ReceivedFile::recoverPending();
    mSocket = new QUdpSocket(this);
    if (!mSocket->bind(QHostAddress::Any, mUdpPort)) emit discoveryError(tr("Discovery unavailable: %1").arg(mSocket->errorString()));
    connect(mSocket, &QUdpSocket::readyRead, this, &DuktoProtocol::newUdpData);
    mTcpServer = new QTcpServer(this);
    if (!mTcpServer->listen(QHostAddress::Any, mTcpPort)) emit discoveryError(tr("Cannot receive: %1").arg(mTcpServer->errorString()));
    connect(mTcpServer, &QTcpServer::newConnection, this, &DuktoProtocol::newIncomingConnection);
    mTimeout = new QTimer(this);
    mTimeout->setSingleShot(true);
    connect(mTimeout, &QTimer::timeout, this, [this] {
        if (mSending) finishSend(tr("Connection timed out. Check the other device, firewall, and Wi-Fi network."));
        else if (mReceiving) finishReceive(false, tr("The sender stopped responding."));
        else rejectIncomingTransfer();
    });
    auto *expiry = new QTimer(this);
    connect(expiry, &QTimer::timeout, this, [this] {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        for (const auto &ip : mLastSeen.keys()) if (now - mLastSeen[ip] > 150000) {
            emit peerListRemoved(mPeers.take(ip)); mLastSeen.remove(ip);
        }
    });
    expiry->start(30000);
}
QString DuktoProtocol::signature() const {
    return Platform::getSystemUsername() + " at " + Platform::getHostname() + " (" + Platform::getPlatformName() + ')';
}
void DuktoProtocol::sayHello(QHostAddress dest) { sayHello(dest, qint16(mUdpPort)); }
void DuktoProtocol::sayHello(QHostAddress dest, qint16 port) {
    if (!mSocket) return;
    QByteArray packet;
    const bool extended = quint16(port) != 4644 || mUdpPort != 4644;
    packet.append(char(dest == QHostAddress::Broadcast ? (extended ? 4 : 1) : (extended ? 5 : 2)));
    if (extended) packet.append(reinterpret_cast<const char *>(&mUdpPort), 2);
    packet.append(signature().toUtf8());
    if (dest == QHostAddress::Broadcast) {
        sendToAllBroadcast(packet, quint16(port));
        if (quint16(port) != 4644) sendToAllBroadcast(packet,4644);
    } else mSocket->writeDatagram(packet,dest,quint16(port));
}
void DuktoProtocol::sayGoodbye() {
    if (!mSocket) return;
    QSet<quint16> ports{mUdpPort, 4644};
    for (const auto &peer : mPeers) ports.insert(quint16(peer.port));
    for (quint16 port : ports) sendToAllBroadcast(QByteArray("\3Bye Bye"), port);
}
void DuktoProtocol::updateBuddyName() { sayGoodbye(); sayHello(QHostAddress::Broadcast); }
void DuktoProtocol::sendToAllBroadcast(const QByteArray &packet, quint16 port) {
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::CanBroadcast) || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const auto &address : iface.addressEntries())
            if (address.ip().protocol() == QAbstractSocket::IPv4Protocol && !address.broadcast().isNull())
                mSocket->writeDatagram(packet, address.broadcast(), port);
    }
}
void DuktoProtocol::newUdpData()
{
    // Refresh once per batch so interface changes are picked up without polling.
    const auto localAddresses = QNetworkInterface::allAddresses();
    while (mSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        const qint64 pendingSize = mSocket->pendingDatagramSize();
        if (pendingSize < 0) break;
        const int maxLength = qMin<qint64>(pendingSize, 65536);
        datagram.resize(maxLength);
        QHostAddress sender;
        quint16 senderPort;
        int size = mSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        if (size < 0) break;
        if (size == 0) continue;
        datagram.resize(size);
        handleMessage(datagram, sender, localAddresses);
    }
}

void DuktoProtocol::handleMessage(QByteArray &data, QHostAddress &sender,
                                 const QList<QHostAddress> &localAddresses)
{
    if (data.isEmpty() || sender.isNull() || sender.isLoopback()) return;
    for (const auto &address : localAddresses) {
        // A dual-stack socket may report IPv4 senders as ::ffff:a.b.c.d.
        if (sender.isEqual(address, QHostAddress::ConvertV4MappedToIPv4)) return;
    }

    char msgtype = data.at(0);
    const QString key = sender.toString();
    if (msgtype == 0x03) {
        const auto peer = mPeers.find(key);
        if (peer != mPeers.end()) {
            const Peer removed = peer.value();
            mPeers.erase(peer);
            emit peerListRemoved(removed);
        }
        mLastSeen.remove(key);
        return;
    }

    data.remove(0, 1);
    quint16 port = 4644;
    if (msgtype == 0x04 || msgtype == 0x05) {
        if (data.size() < int(sizeof(port)) + 1) return;
        std::memcpy(&port, data.constData(), sizeof(port));
        data.remove(0, sizeof(port));
    } else if (msgtype != 0x01 && msgtype != 0x02) {
        return;
    }
    if (port == 0 || data.isEmpty() || data.size() > 4096) return;

    const Peer peer(sender, QString::fromUtf8(data), qint16(port));
    mPeers.insert(key, peer);
    mLastSeen.insert(key, QDateTime::currentMSecsSinceEpoch());
    if (msgtype == 0x01 || msgtype == 0x04) sayHello(sender, qint16(port));
    emit peerListAdded(peer);
}

void DuktoProtocol::newIncomingConnection() {
    while (mTcpServer->hasPendingConnections()) {
        QTcpSocket *socket = mTcpServer->nextPendingConnection();
        if (mBusy || !mReceivingEnabled) { socket->abort(); socket->deleteLater(); continue; }
        mBusy = true;
        mPendingIncomingSocket = socket;
        socket->setReadBufferSize(Backlog);
        connect(socket,&QTcpSocket::disconnected,this,[this,socket] {
            if (mPendingIncomingSocket == socket && !socket->bytesAvailable()) rejectIncomingTransfer();
        });
        mTimeout->start(30000);
        emit incomingTransferRequested(socket->peerAddress().toString());
    }
}
void DuktoProtocol::acceptIncomingTransfer() {
    if (!mPendingIncomingSocket) return;
    mCurrentSocket = mPendingIncomingSocket; mPendingIncomingSocket = nullptr;
    mActiveReceivePath = mReceivePath;
    mActiveReceiveTree = ReceivedFile::selectedTree();
    mReceiveError.clear();
    useCancellation(std::make_shared<std::atomic_bool>(false));
    mReceiving = true; mReceiveState = Header;
    mTotal = mTransferred = mExpectedElements = mCompletedElements = mRemaining = 0;
    mName.clear(); mReceivedText.clear(); mReceivedFiles.clear(); mRootNames.clear();
    mTextElement = mSawText = false; mOutput.reset(); mStatusClock.invalidate();
    connect(mCurrentSocket,&QTcpSocket::readyRead,this,&DuktoProtocol::readNewData);
    connect(mCurrentSocket,&QTcpSocket::disconnected,this,&DuktoProtocol::closedConnection);
    mTimeout->start(30000);
    emit receiveFileStart(mCurrentSocket->peerAddress().toString());
    readNewData();
    if (mReceiving && mCurrentSocket && mCurrentSocket->state() == QAbstractSocket::UnconnectedState) closedConnection();
}
void DuktoProtocol::rejectIncomingTransfer() {
    if (!mPendingIncomingSocket) return;
    auto *socket = mPendingIncomingSocket; mPendingIncomingSocket = nullptr;
    socket->disconnect(this); socket->abort(); socket->deleteLater();
    mTimeout->stop(); mBusy = false; emit incomingTransferDismissed();
}
QString DuktoProtocol::uniqueDestination(const QString &path) const {
    QFileInfo info(path);
    QString candidate = path;
    for (int i = 2; QFileInfo::exists(candidate) || QFileInfo(candidate).isSymLink(); ++i) {
        const QString suffix = info.completeSuffix();
        candidate = info.dir().absoluteFilePath(info.completeBaseName() + QString(" (%1)").arg(i) + (suffix.isEmpty() ? "" : "." + suffix));
    }
    return candidate;
}
bool DuktoProtocol::safeDestination(const QString &relative, QString *absolute) const {
    if (relative.isEmpty() || relative.size() > 4096 || relative.contains('\\') || relative.contains(':') || QDir::isAbsolutePath(relative)) return false;
    const auto parts = relative.split('/');
    for (const QString &part : parts) {
        if (part.isEmpty() || part == "." || part == ".." || part.endsWith('.') || part.endsWith(' ')) return false;
        for (const QChar ch : part) if (ch.unicode() < 32 || QStringLiteral("<>\"|?*").contains(ch)) return false;
        static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)",QRegularExpression::CaseInsensitiveOption);
        if (reserved.match(part).hasMatch()) return false;
    }
    const QString root = QFileInfo(mActiveReceivePath).canonicalFilePath();
    if (root.isEmpty() || !QFileInfo(root).isDir()) return false;
    QString path = root;
    for (const auto &part : parts) {
        path += '/' + part;
        QFileInfo info(path);
        if (info.isSymLink()) return false;
        if (info.exists()) {
            const auto canonical = info.canonicalFilePath();
            if (!canonical.startsWith(root + '/')) return false;
        }
    }
    *absolute = path; return true;
}
bool DuktoProtocol::beginElement(const QString &name, qint64 size) {
    if (mCompletedElements >= mExpectedElements || size < -1 || (size >= 0 && size > mTotal - mTransferred)) return false;
    mTextElement = name == TextName;
    if (mTextElement) {
        if (mExpectedElements != 1 || size < 0 || size > MaxText) return false;
        mSawText = true;
    } else {
        QString destination;
        if (!safeDestination(name,&destination)) return false;
        QStringList parts = name.split('/');
        const QString rootName = parts.first();
        if (!mRootNames.contains(rootName)) {
#ifdef Q_OS_ANDROID
            const QString root = size == -1 || parts.size() > 1
                ? ReceivedFile::sharedRootDirectory(rootName, mActiveReceiveTree, &mReceiveError) : rootName;
            if (root.isEmpty()) return false;
            mRootNames.insert(rootName, root);
#else
            const QString rootPath = QDir(mActiveReceivePath).absoluteFilePath(rootName);
            mRootNames.insert(rootName,QFileInfo(uniqueDestination(rootPath)).fileName());
#endif
        }
        parts[0] = mRootNames.value(rootName);
        if (!safeDestination(parts.join('/'),&destination)) return false;
        if (size == -1) {
#ifdef Q_OS_ANDROID
            if (parts.size() > 1 && !ReceivedFile::sharedDirectory(parts.join('/'), mActiveReceiveTree, &mReceiveError)) return false;
#else
            if (!QDir().mkpath(destination)) return false;
            mReceivedFiles.append(destination);
#endif
        } else {
#ifdef Q_OS_ANDROID
            mOutput = ReceivedFile::shared(parts.join('/'), size, mActiveReceiveTree, &mReceiveError);
#else
            if (!QDir().mkpath(QFileInfo(destination).absolutePath())) return false;
            destination = uniqueDestination(destination);
            if (QStorageInfo(QFileInfo(destination).absolutePath()).bytesAvailable() < size) return false;
            mOutput = ReceivedFile::local(destination, &mReceiveError);
            mReceivedFiles.append(destination);
#endif
            if (!mOutput) return false;
        }
    }
    emit currentFileChanged(name);
    mCurrentElementSize = size;
    mRemaining = qMax<qint64>(0,size);
    mReceiveState = Data;
    return true;
}
void DuktoProtocol::readNewData() {
    if (!mReceiving || !mCurrentSocket) return;
    // A complete empty file/folder must advance even without another byte arriving.
    while (mReceiving && mCurrentSocket) {
        if (mCancellation->load()) { finishReceive(false,tr("Transfer cancelled.")); return; }
        if (mReceiveState == Header) {
            if (mCurrentSocket->bytesAvailable() < 16) return;
            mExpectedElements = takeNumber(mCurrentSocket); mTotal = takeNumber(mCurrentSocket);
            if (mExpectedElements < 1 || mExpectedElements > MaxElements || mTotal < 0) { finishReceive(false,tr("Invalid transfer header.")); return; }
            mReceiveState = Filename;
        } else if (mReceiveState == Filename) {
            if (mCompletedElements == mExpectedElements) {
                if (mCurrentSocket->bytesAvailable()) finishReceive(false,tr("Unexpected data after the transfer."));
                return;
            }
            char ch;
            if (mCurrentSocket->read(&ch,1) != 1) return;
            if (ch == '\0') mReceiveState = FileSize;
            else { mName.append(ch); if (mName.size() > 4096) { finishReceive(false,tr("Filename is too long.")); return; } }
        } else if (mReceiveState == FileSize) {
            if (mCurrentSocket->bytesAvailable() < 8) return;
            const auto size = takeNumber(mCurrentSocket);
            QString name = QString::fromUtf8(mName);
            if (name.toUtf8() != mName || !beginElement(name,size)) { finishReceive(false,mReceiveError.isEmpty() ? tr("Invalid filename, insufficient storage, or an unwritable destination.") : mReceiveError); return; }
            mName.clear();
        } else {
            if (!mRemaining) {
                if (mOutput) {
                    if (!mOutput->commit()) { finishReceive(false,mOutput->errorString()); return; }
#ifdef Q_OS_ANDROID
                    mReceivedFiles.append(mOutput->location());
#endif
                    emit receivedFilePublished(mOutput->location(), mOutput->displayName(), mCurrentElementSize);
                }
                mOutput.reset(); ++mCompletedElements; mReceiveState = Filename; continue;
            }
            const qint64 count = qMin(Chunk,qMin(mRemaining,mCurrentSocket->bytesAvailable()));
            if (!count) return;
            QByteArray bytes = mCurrentSocket->read(count);
            if (bytes.isEmpty()) return;
            if (mTextElement) mReceivedText.append(bytes);
            else {
                qint64 offset = 0;
                while (offset < bytes.size()) {
                    auto written = mOutput->write(bytes.constData()+offset,bytes.size()-offset);
                    if (written <= 0) { finishReceive(false,tr("Disk write failed.")); return; }
                    offset += written;
                }
            }
            mRemaining -= bytes.size(); mTransferred += bytes.size(); updateStatus();
        }
        mTimeout->start(30000);
    }
}
void DuktoProtocol::closedConnection() {
    readNewData();
    if (!mReceiving) return;
    const bool complete = mReceiveState == Filename && mName.isEmpty() && mCompletedElements == mExpectedElements && mTransferred == mTotal;
    finishReceive(complete,complete ? QString() : tr("Transfer interrupted. Incomplete files were discarded."));
}
void DuktoProtocol::finishReceive(bool success, const QString &error) {
    const auto files = mReceivedFiles;
    const auto text = QString::fromUtf8(mReceivedText);
    const auto total = mTotal;
    const bool isText = mSawText;
    updateStatus(true); mOutput.reset(); resetSocket(); mReceiving = false; mBusy = false;
    mReceivedText.clear(); mReceivedFiles.clear(); mRootNames.clear();
    if (!success) { emit transferError(error); emit receiveFileCancelled(); }
    else if (isText) emit receiveTextComplete(text,total);
    else emit receiveFileComplete(files,total);
}
void DuktoProtocol::sendRequest(const TransferRequest &request) {
    if (mBusy) { emit transferError(tr("The receiver is busy. Retry this job.")); emit sendFileError(-1); return; }
    mBusy = true; mSending = true; mRequest = request;
    useCancellation(request.cancellation);
    mSourceIndex = 0; mSourceRemaining = 0; mTotal = mTransferred = 0;
    mText.clear(); mTextOffset = 0; mOutgoing.clear(); mSources.clear(); mStatusClock.invalidate();
    QString error;
    if (request.isText) {
        mText = request.text.toUtf8(); mTotal = mText.size();
        if (mTotal > MaxText) { finishSend(tr("Text exceeds the 16 MiB limit.")); return; }
    } else {
        mSources = prepareSources(request.sources,&error,*mCancellation);
        if (mCancellation->load()) { finishSend({},true); return; }
        if (!error.isEmpty()) { finishSend(error); return; }
        for (const auto &source : mSources) {
            if (!source.directory) {
                if (source.size > std::numeric_limits<qint64>::max()-mTotal) { finishSend(tr("Selection is too large.")); return; }
                mTotal += source.size;
            }
        }
        if (mSources.size() > MaxElements) { finishSend(tr("Too many files in one transfer.")); return; }
    }
    if (mCancellation->load()) { finishSend({},true); return; }
    mCurrentSocket = new QTcpSocket(this);
    connect(mCurrentSocket,&QTcpSocket::connected,this,&DuktoProtocol::sendMetaData);
    connect(mCurrentSocket,&QTcpSocket::bytesWritten,this,&DuktoProtocol::sendData);
    connect(mCurrentSocket,&QTcpSocket::errorOccurred,this,&DuktoProtocol::sendConnectError);
    connect(mCurrentSocket,&QTcpSocket::disconnected,this,[this] {
        if (mSending) finishSend(tr("The receiver disconnected before the transfer finished."));
    });
    mTimeout->start(30000);
    mCurrentSocket->connectToHost(request.host,request.port);
}
void DuktoProtocol::sendMetaData() {
    appendNumber(mOutgoing,mRequest.isText ? 1 : mSources.size()); appendNumber(mOutgoing,mTotal);
    sendData();
}
void DuktoProtocol::sendData(qint64) {
    if (!mSending || !mCurrentSocket) return;
    if (mCancellation->load()) { finishSend({},true); return; }
    mTimeout->start(30000);
    while (mCurrentSocket->bytesToWrite() < Backlog) {
        if (!mOutgoing.isEmpty()) {
            const auto count = mCurrentSocket->write(mOutgoing.constData(),qMin<qint64>(mOutgoing.size(),Backlog-mCurrentSocket->bytesToWrite()));
            if (count < 0) { finishSend(mCurrentSocket->errorString()); return; }
            if (!count) return;
            mOutgoing.remove(0,count); continue;
        }
        if (mRequest.isText) {
            if (mSourceIndex == 0) {
                mOutgoing = TextName.toUtf8(); mOutgoing.append('\0'); appendNumber(mOutgoing,mTotal); ++mSourceIndex; continue;
            }
            if (mTextOffset < mText.size()) {
                mOutgoing = mText.mid(mTextOffset,Chunk); mTextOffset += mOutgoing.size(); mTransferred += mOutgoing.size(); updateStatus(); continue;
            }
        } else {
            if (mSourceRemaining > 0) {
                auto &source = mSources[mSourceIndex-1];
                mOutgoing.resize(qMin(Chunk,mSourceRemaining));
                auto count = readSource(source,mOutgoing.data(),mOutgoing.size(),*mCancellation);
                if (mCancellation->load()) { finishSend({},true); return; }
                if (count <= 0) { finishSend(tr("Could not read %1. The file may have changed or access expired.").arg(source.name)); return; }
                mOutgoing.resize(count); mSourceRemaining -= count; mTransferred += count; updateStatus();
                if (!mSourceRemaining) source.stream.reset();
                continue;
            }
            if (mSourceIndex < mSources.size()) {
                auto &source = mSources[mSourceIndex++];
                emit currentFileChanged(source.name);
                QString error;
                if (!openSource(source,&error)) { finishSend(error); return; }
                mOutgoing = source.name.toUtf8(); mOutgoing.append('\0'); appendNumber(mOutgoing,source.directory ? -1 : source.size);
                mSourceRemaining = qMax<qint64>(0,source.size);
                if (!mSourceRemaining) source.stream.reset();
                continue;
            }
        }
        if (!mCurrentSocket->bytesToWrite()) finishSend();
        return;
    }
}
void DuktoProtocol::sendConnectError(QAbstractSocket::SocketError) {
    if (mSending) finishSend(mCurrentSocket ? mCurrentSocket->errorString() : tr("Connection failed."));
}
void DuktoProtocol::finishSend(const QString &error, bool cancelled) {
    if (!mSending) return;
    updateStatus(true); resetSocket(); mSources.clear(); mOutgoing.clear(); mText.clear();
    mRequest.temporary.reset(); mRequest.text.clear(); mRequest.sources.clear();
    mSending = false; mBusy = false;
    if (cancelled) emit sendFileAborted();
    else if (!error.isEmpty()) { emit transferError(error); emit sendFileError(-1); }
    else emit sendFileComplete();
}
void DuktoProtocol::resetSocket() {
    if (mTimeout) mTimeout->stop();
    if (mCurrentSocket) {
        mCurrentSocket->disconnect(this);
        mCurrentSocket->disconnectFromHost();
        mCurrentSocket->deleteLater(); mCurrentSocket = nullptr;
    }
}
void DuktoProtocol::abortCurrentTransfer() {
    if (!mBusy) return;
    mCancellation->store(true);
    if (mSending) finishSend({},true);
    else if (mReceiving) finishReceive(false,tr("Transfer cancelled."));
    else rejectIncomingTransfer();
}
void DuktoProtocol::abortRequest(const QString &id) {
    // Queued commands from an earlier job must never cancel its successor.
    if (!mBusy || (mSending && mRequest.id != id) || (!mSending && !id.isEmpty())) return;
    abortCurrentTransfer();
}
void DuktoProtocol::updateStatus(bool force) {
    if (!force && mStatusClock.isValid() && mStatusClock.elapsed() < 100) return;
    mStatusClock.start(); emit transferStatusUpdate(mTotal,mTransferred);
}

void DuktoProtocol::useCancellation(std::shared_ptr<std::atomic_bool> cancellation) {
    QMutexLocker lock(&mCancellationMutex);
    mCancellation = std::move(cancellation);
}
void DuktoProtocol::requestCancel() {
    QMutexLocker lock(&mCancellationMutex);
    mCancellation->store(true);
}
