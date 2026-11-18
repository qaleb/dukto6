#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
#include <QElapsedTimer>
#include <QTimer>
#include <QHash>
#include "receivedfile.h"
#include <atomic>
#include <QMutex>
#include "peer.h"
#include "transferrequest.h"
#include "transfersource.h"

class DuktoProtocol : public QObject {
    Q_OBJECT
public:
    explicit DuktoProtocol(QObject *parent = nullptr);
    ~DuktoProtocol() override;
    void initialize();
    void setPorts(qint16 udp, qint16 tcp);
    void sayHello(QHostAddress dest);
    void sayHello(QHostAddress dest, qint16 port);
    void sayGoodbye();
    QHash<QString, Peer> &getPeers() { return mPeers; }
    bool isBusy() const { return mBusy.load(); }
    void sendRequest(const TransferRequest &request);
    void setReceivePath(const QString &path) { mReceivePath = path; }
    void setReceivingEnabled(bool enabled) { mReceivingEnabled = enabled; }
    void updateBuddyName();
    // The cancellation flag can interrupt synchronous source preparation on the worker.
    void requestCancel();
public slots:
    void newUdpData();
    void newIncomingConnection();
    void readNewData();
    void closedConnection();
    void acceptIncomingTransfer();
    void rejectIncomingTransfer();
    void abortCurrentTransfer();
    void abortRequest(const QString &id);
    void sendMetaData();
    void sendData(qint64 bytes = 0);
    void sendConnectError(QAbstractSocket::SocketError error);
signals:
    void peerListAdded(Peer peer);
    void peerListRemoved(Peer peer);
    void sendFileComplete();
    void sendFileError(int code);
    void sendFileAborted();
    void transferError(QString message);
    void currentFileChanged(QString name);
    void receivedFilePublished(QString location, QString name, qint64 size);
    void receiveFileStart(QString senderIp);
    void incomingTransferRequested(QString senderIp);
    void incomingTransferDismissed();
    void receiveFileComplete(QStringList files, qint64 totalSize);
    void receiveTextComplete(QString text, qint64 totalSize);
    void receiveFileCancelled();
    void transferStatusUpdate(qint64 total, qint64 partial);
    void discoveryError(QString message);
private:
    void useCancellation(std::shared_ptr<std::atomic_bool> cancellation);
    friend class DiscoveryTest;
    QString getSystemSignature() const { return signature(); }
    QString signature() const;
    void sendToAllBroadcast(const QByteArray &packet, quint16 port);
    void handleMessage(QByteArray &data, QHostAddress &sender,
                       const QList<QHostAddress> &localAddresses);
    void finishSend(const QString &error = {}, bool cancelled = false);
    void finishReceive(bool success, const QString &error = {});
    void resetSocket();
    void updateStatus(bool force = false);
    bool beginElement(const QString &name, qint64 size);
    bool safeDestination(const QString &relative, QString *absolute) const;
    QString uniqueDestination(const QString &path) const;
    QUdpSocket *mSocket = nullptr;
    QTcpServer *mTcpServer = nullptr;
    QTcpSocket *mCurrentSocket = nullptr;
    QTcpSocket *mPendingIncomingSocket = nullptr;
    QTimer *mTimeout = nullptr;
    QHash<QString, Peer> mPeers;
    QHash<QString, qint64> mLastSeen;
    quint16 mUdpPort = 4644, mTcpPort = 4644;
    std::atomic_bool mBusy{false};
    QMutex mCancellationMutex;
    std::shared_ptr<std::atomic_bool> mCancellation = std::make_shared<std::atomic_bool>(false);
    bool mSending = false, mReceiving = false, mReceivingEnabled = true;
    QString mReceivePath;
    QString mActiveReceivePath;
    QString mActiveReceiveTree, mReceiveError;
    qint64 mCurrentElementSize = 0;
    QElapsedTimer mStatusClock;
    TransferRequest mRequest;
    QList<TransferSource> mSources;
    int mSourceIndex = 0;
    qint64 mSourceRemaining = 0;
    QByteArray mOutgoing, mText;
    qsizetype mTextOffset = 0;
    qint64 mTotal = 0, mTransferred = 0;
    enum ReceiveState { Header, Filename, FileSize, Data } mReceiveState = Header;
    QByteArray mName, mReceivedText;
    QStringList mReceivedFiles;
    QHash<QString, QString> mRootNames;
    std::unique_ptr<ReceivedFile> mOutput;
    qint64 mExpectedElements = 0, mCompletedElements = 0, mRemaining = 0;
    bool mTextElement = false, mSawText = false;
};
