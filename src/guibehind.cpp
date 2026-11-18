#include "guibehind.h"
#include "platform.h"
#include "winhelper.h" // Add this include

#include <QDebug>
#include <QQmlContext>
#include <QQuickView>
#include <QQmlProperty>
#include <QDir>
#include <QUrl>
#include <QDesktopServices>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QThread>
#include <QPointer>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QClipboard>
#include <QGuiApplication>
#if !defined(Q_OS_ANDROID)
#include <QFileDialog>
#include <QApplication>
#endif
#include <QScreen>
#if defined(Q_OS_WIN)
#include <QWindow>
#include <windows.h>
#include <ShObjIdl.h>
#endif

#define NETWORK_PORT 4644 // 6742

#if defined(Q_OS_ANDROID)
#include <QJniEnvironment>
#endif

// The constructor is private and can only be called within the singleton instance method
GuiBehind::GuiBehind(QQmlApplicationEngine &engine, QObject *parent, quint16 port) :
    QObject(parent), mShowBackTimer(NULL), mPeriodicHelloTimer(NULL), mClipboard(NULL),
    mMiniWebServer(NULL), mSettings(this), mDestBuddy(NULL), mUpdatesChecker(NULL)
{
    // Other platforms can be re-added here if needed

    // Change current folder
    const QString initialPath = currentPath();
    QDir().mkpath(initialPath);
    QDir::setCurrent(initialPath);

    // QString rootPath = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).value(0);
    // qDebug() << "The general download path is:" << rootPath;

    // Status variables
    // mView->setGuiBehindReference(this);
    setCurrentTransferProgress(0);
    setTextSnippetSending(false);
    setShowUpdateBanner(false);

    // Clipboard object
    mClipboard = QGuiApplication::clipboard();
    connect(mClipboard, SIGNAL(dataChanged()), this, SLOT(clipboardChanged()));
    clipboardChanged(); // Initial call to handle current clipboard data

    // Add "Me" entry
    mBuddiesList.addMeElement();

    // Add "Ip" entry
    mBuddiesList.addIpElement();

    // Destination buddy
    mDestBuddy = new DestinationBuddy(this);

    // Mini web server
    if (port) {
        mMiniWebServer = new MiniWebServer(port + 1);
        mMiniWebServer->setParent(this);
    }

    // Initialize and set the current theme color
    mTheme.setThemeColor(mSettings.themeColor());
    mTheme.setMode(mSettings.themeMode());

    // Expose instances to QML context
    engine.rootContext()->setContextProperty("buddiesListData", &mBuddiesList);
    engine.rootContext()->setContextProperty("recentListData", &mRecentList);
    engine.rootContext()->setContextProperty("ipAddressesData", &mIpAddresses);
    engine.rootContext()->setContextProperty("guiBehind", this);
    engine.rootContext()->setContextProperty("destinationBuddy", mDestBuddy);
    engine.rootContext()->setContextProperty("theme", &mTheme);

    mDuktoProtocol = new DuktoProtocol;
    mDuktoProtocol->moveToThread(&mProtocolThread);
    connect(&mProtocolThread, &QThread::finished, mDuktoProtocol, &QObject::deleteLater);
    connect(&mTransfers, &TransferQueue::startRequested, this, &GuiBehind::startJob);
    connect(mDuktoProtocol, &DuktoProtocol::transferError, this,
            [this](const QString &error) { mLastTransferError = error; });
    connect(mDuktoProtocol, &DuktoProtocol::discoveryError, this,
            [](const QString &error) { qWarning().noquote() << error; });
    connect(mDuktoProtocol, &DuktoProtocol::incomingTransferDismissed,
            this, &GuiBehind::dismissIncomingTransfer);
    connect(&mAndroidShares, &AndroidShareBridge::shareReceived, this, &GuiBehind::stageShare);
    connect(&mAndroidShares, &AndroidShareBridge::storageChanged, this, &GuiBehind::androidStorageChanged);
    connect(&mAndroidShares, &AndroidShareBridge::keyboardTopChanged, this, [this](int pixels) {
        if (mAndroidKeyboardTopPixels == pixels) return;
        mAndroidKeyboardTopPixels = pixels;
        emit androidKeyboardChanged();
    });
#if defined(Q_OS_ANDROID)
    connect(mDuktoProtocol, &DuktoProtocol::receivedFilePublished, this,
            [this](const QString &uri, const QString &name, qint64 size) {
        mRecentList.addRecent(name, uri, "file", mCurrentTransferBuddy, size);
    });
#endif
    connect(&mAndroidShares, &AndroidShareBridge::serviceUnavailable, this, [this] {
#if defined(Q_OS_ANDROID)
        mAndroidServiceActive = false;
#endif
        mTransfers.setPaused(true);
        abortTransfer();
        showError(tr("Android stopped the transfer service. Return to Dukto to continue queued transfers."));
    });
    mProtocolThread.start();

    // Register protocol signals
    connect(mDuktoProtocol, SIGNAL(peerListAdded(Peer)), this, SLOT(peerListAdded(Peer)));
    connect(mDuktoProtocol, SIGNAL(peerListRemoved(Peer)), this, SLOT(peerListRemoved(Peer)));
    connect(mDuktoProtocol, SIGNAL(incomingTransferRequested(QString)), this, SLOT(incomingTransferRequested(QString)));
    connect(mDuktoProtocol, SIGNAL(receiveFileStart(QString)), this, SLOT(receiveFileStart(QString)));
    connect(mDuktoProtocol, SIGNAL(transferStatusUpdate(qint64,qint64)), this, SLOT(transferStatusUpdate(qint64,qint64)));
    connect(mDuktoProtocol, SIGNAL(receiveFileComplete(QStringList,qint64)), this, SLOT(receiveFileComplete(QStringList,qint64)));
    connect(mDuktoProtocol, SIGNAL(receiveTextComplete(QString,qint64)), this, SLOT(receiveTextComplete(QString,qint64)));
    connect(mDuktoProtocol, SIGNAL(sendFileComplete()), this, SLOT(sendFileComplete()));
    connect(mDuktoProtocol, SIGNAL(sendFileError(int)), this, SLOT(sendFileError(int)));
    connect(mDuktoProtocol, SIGNAL(receiveFileCancelled()), this, SLOT(receiveFileCancelled()));
    connect(mDuktoProtocol, SIGNAL(sendFileAborted()), this, SLOT(sendFileAborted()));

    // Register other signals
    connect(this, SIGNAL(remoteDestinationAddressChanged()), this, SLOT(remoteDestinationAddressHandler()));

    // Say "hello"
    const QString destination = currentPath();
    QDir().mkpath(destination);
    QMetaObject::invokeMethod(mDuktoProtocol, [this, port, destination] {
        mDuktoProtocol->setPorts(qint16(port), qint16(port));
        mDuktoProtocol->setReceivePath(destination);
        mDuktoProtocol->initialize();
        if (port) mDuktoProtocol->sayHello(QHostAddress::Broadcast);
    });

    // Periodic "hello" timer
    mPeriodicHelloTimer = new QTimer(this);
    connect(mPeriodicHelloTimer, SIGNAL(timeout()), this, SLOT(periodicHello()));
    mPeriodicHelloTimer->start(60000);

    // Start random rotate
    mShowBackTimer = new QTimer(this);
    connect(mShowBackTimer, SIGNAL(timeout()), this, SLOT(showRandomBack()));
    uint iSeed = QDateTime::currentSecsSinceEpoch();
    srand(iSeed);
    mShowBackTimer->start(10000);

    // Enqueue check for updates
    mUpdatesChecker = new UpdatesChecker(this);
    connect(mUpdatesChecker, SIGNAL(updatesAvailable()), this, SLOT(showUpdatesMessage()));
    QTimer::singleShot(2000, mUpdatesChecker, SLOT(start()));

#ifdef Q_OS_WIN
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        createActions();
        createTrayIcon();
        if (trayIcon)
            trayIcon->show();
    }
#endif

#if defined(Q_OS_ANDROID)
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        const bool foreground = state == Qt::ApplicationActive;
        if (foreground) {
            mShowBackTimer->start(10000);
            mPeriodicHelloTimer->start(60000);
        } else {
            mShowBackTimer->stop();
            mPeriodicHelloTimer->stop();
        }
        mTransfers.setPaused(!foreground && !mAndroidServiceActive);
        QMetaObject::invokeMethod(mDuktoProtocol, [this, foreground] {
            mDuktoProtocol->setReceivingEnabled(foreground);
            if (foreground) mDuktoProtocol->sayHello(QHostAddress::Broadcast);
            else mDuktoProtocol->sayGoodbye();
        });
    });
#endif
}

#if defined(Q_OS_ANDROID)
void GuiBehind::setAndroidTransferActive(bool active)
{
    if (mAndroidServiceActive == active) return;
    mAndroidServiceActive = active;
    if (!active && QGuiApplication::applicationState() != Qt::ApplicationActive) mTransfers.setPaused(true);
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) return;
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoBridge", "setTransferForeground",
        "(Landroid/content/Context;Z)V", context.object<jobject>(), jboolean(active));
}



#endif

QRect GuiBehind::windowGeometry()
{
    return mSettings.windowRect();
}

qreal GuiBehind::androidKeyboardTop() const
{
    if (mAndroidKeyboardTopPixels < 0) return -1;
    const auto *screen = QGuiApplication::primaryScreen();
    return mAndroidKeyboardTopPixels / (screen ? screen->devicePixelRatio() : 1.0);
}

void GuiBehind::setWindowGeometry(QRect geo)
{
    mSettings.saveWindowGeometry(geo);
}

// Add the new buddy to the buddy list
void GuiBehind::peerListAdded(Peer peer) {
    mBuddiesList.addBuddy(peer);
}

// Remove the buddy from the buddy list
void GuiBehind::peerListRemoved(Peer peer) {

    // Check if currently is shown the "send" page for that buddy
    if (((overlayState() == "send")
         || ((overlayState() == "showtext") && textSnippetSending()))
        && (mDestBuddy->ip() == peer.address.toString()))
        emit hideAllOverlays();

    // Check if currently is shown the "transfer complete" message box
    // for the removed user as destination
    if ((overlayState() == "message") && (messagePageBackState() == "send")
        && (mDestBuddy->ip() == peer.address.toString()))
        setMessagePageBackState("");

    // Remove from the list
    mBuddiesList.removeBuddy(peer.address.toString());
}

void GuiBehind::showRandomBack()
{
    // Look for a random element
    int i = QRandomGenerator::global()->bounded(mBuddiesList.rowCount());

    // Show back
    if (i < mBuddiesList.rowCount()) mBuddiesList.showSingleBack(i);
}

void GuiBehind::clipboardChanged()
{
    mClipboardTextAvailable = (mClipboard->text() != "");
    emit clipboardTextAvailableChanged();
}

void GuiBehind::receiveFileStart(QString senderIp)
{
    mActiveJobId.clear();
    mActiveCancellation.reset();
    // Look for the sender in the buddy list
    QString sender = mBuddiesList.buddyNameByIp(senderIp);
    if (sender == "")
        setCurrentTransferBuddy("remote sender");
    else
        setCurrentTransferBuddy(sender);

    // Update user interface
    setCurrentTransferSending(false);
#if defined(Q_OS_ANDROID)
    setAndroidTransferActive(true);
#endif
    // mView->win7()->setProgressValue(0, 100);
    // mView->win7()->setProgressState(EcWin7::Normal);

    emit transferStart();
}

void GuiBehind::incomingTransferRequested(QString senderIp)
{
    mTransfers.setReceiving(true);
    mLastTransferError.clear();
    if (!confirmBeforeReceive()) {
        QMetaObject::invokeMethod(mDuktoProtocol, &DuktoProtocol::acceptIncomingTransfer);
        return;
    }
    mPendingIncomingSender = senderIp;
    emit pendingIncomingSenderChanged();
    emit gotoConfirmReceivePage();
}

void GuiBehind::transferStatusUpdate(qint64 total, qint64 partial)
{
    // Stats formatting
    if (total < 1024)
        setCurrentTransferStats(QString::number(partial) + " B of " + QString::number(total) + " B");
    else if (total < 1048576)
        setCurrentTransferStats(QString::number(partial * 1.0 / 1024, 'f', 1) + " KB of " + QString::number(total * 1.0 / 1024, 'f', 1) + " KB");
    else
        setCurrentTransferStats(QString::number(partial * 1.0 / 1048576, 'f', 1) + " MB of " + QString::number(total * 1.0 / 1048576, 'f', 1) + " MB");

    const int percent = total > 0 ? qBound(0, int(100.0 * partial / total), 100) : 0;
    setCurrentTransferProgress(percent);

#if defined(Q_OS_WIN)
    // Qt 6.9+: QWinTaskbarButton/Progress is removed, so do nothing here.
    // Optionally, you can implement Windows 10 taskbar progress using native Win32 API if needed.
    // For now, just leave this block empty.
#endif
    // mView->win7()->setProgressValue(percent, 100);
}

void GuiBehind::receiveFileComplete(QStringList files, qint64 totalSize) {

    // Add an entry to recent activities
#if !defined(Q_OS_ANDROID)
    QDir d(".");
    if (files.size() == 1)
        mRecentList.addRecent(files.at(0), d.absoluteFilePath(files.at(0)), "file", mCurrentTransferBuddy, totalSize);
    else
        mRecentList.addRecent(tr("Files and folders"), d.absolutePath(), "misc", mCurrentTransferBuddy, totalSize);
#else
    Q_UNUSED(files); Q_UNUSED(totalSize);
#endif

    // Update GUI
    // mView->win7()->setProgressState(EcWin7::NoProgress);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    //TODO: check QtQuick 2 alert
    QApplication::alert(mView, 5000);
#endif

#if defined(Q_OS_WIN)
    HWND hwnd = WinHelper::WinTaskbarProgressHelper::getMainWindowHandle();
    WinHelper::WinTaskbarProgressHelper::setSuccess(hwnd);
    if (!WinHelper::WinTaskbarProgressHelper::isMainWindowActive()) {
        WinHelper::WinTaskbarProgressHelper::flashWindowIfNotActive(hwnd);
        QTimer::singleShot(1500, this, []() {
            WinHelper::WinTaskbarProgressHelper::clearProgress();
        });
    } else {
        WinHelper::WinTaskbarProgressHelper::clearProgress();
    }
#endif

    mTransfers.setReceiving(false);
#if defined(Q_OS_ANDROID)
    if (!mTransfers.hasWork()) setAndroidTransferActive(false);
#endif
    emit receiveCompleted();
}

void GuiBehind::receiveTextComplete(QString text, qint64 totalSize)
{
    // Add an entry to recent activities
    mRecentList.addRecent(tr("Text snippet"), text, "text", mCurrentTransferBuddy, totalSize);

    // Update GUI
    // mView->win7()->setProgressState(EcWin7::NoProgress);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    //TODO: check QtQuick 2 alert
    QApplication::alert(mView, 5000);
#endif

#if defined(Q_OS_WIN)
    HWND hwnd = WinHelper::WinTaskbarProgressHelper::getMainWindowHandle();
    WinHelper::WinTaskbarProgressHelper::setSuccess(hwnd);
    if (!WinHelper::WinTaskbarProgressHelper::isMainWindowActive()) {
        WinHelper::WinTaskbarProgressHelper::flashWindowIfNotActive(hwnd);
        QTimer::singleShot(1500, this, []() {
            WinHelper::WinTaskbarProgressHelper::clearProgress();
        });
    } else {
        WinHelper::WinTaskbarProgressHelper::clearProgress();
    }
#endif

    mTransfers.setReceiving(false);
#if defined(Q_OS_ANDROID)
    if (!mTransfers.hasWork()) setAndroidTransferActive(false);
#endif
    emit receiveCompleted();
}

void GuiBehind::showTextSnippet(QString text, QString sender)
{
    setTextSnippet(text);
    setTextSnippetBuddy(sender);
    setTextSnippetSending(false);
    emit gotoTextSnippet();
}

void GuiBehind::openFile(QString path)
{
#if defined(Q_OS_ANDROID)
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoBridge", "openReceivedFile",
        "(Landroid/content/Context;Ljava/lang/String;)V", context.object<jobject>(),
        QJniObject::fromString(path).object<jstring>());
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
}

void GuiBehind::openDestinationFolder() {
#if defined(Q_OS_ANDROID)
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoActivity", "browseReceivedFiles",
        "(Landroid/content/Context;)V", context.object<jobject>());
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath()));
#endif
}

void GuiBehind::changeDestinationFolder(QString dirpath)
{
#if defined(Q_OS_WIN)
    if (dirpath.isEmpty()) {
        QString dirname = QFileDialog::getExistingDirectory(nullptr, tr("Change folder"), ".",
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (dirname.isEmpty()) return;
        dirpath = dirname;
    } else {
        QUrl url(dirpath);
        if (url.isLocalFile()) {
            dirpath = url.toLocalFile();
        }
    }
    if (dirpath.isEmpty()) return;
    QDir::setCurrent(dirpath);
    setCurrentPath(dirpath);
#else
    // Convert URL to local file path
    QUrl url(dirpath);
    if (url.isLocalFile()) {
        dirpath = url.toLocalFile();
    }
    if (dirpath.isEmpty()) return;
    QDir::setCurrent(dirpath);
    setCurrentPath(dirpath);
#endif
}

void GuiBehind::refreshIpList()
{
    mIpAddresses.refreshIpList();
}

void GuiBehind::showSendPage(QString ip)
{
    // Check for a buddy with the provided IP address
    QStandardItem *buddy = mBuddiesList.buddyByIp(ip);
    if (buddy == NULL) return;

    // Update exposed data for the selected user
    mDestBuddy->fillFromItem(buddy);

    // Preventive update of destination buddy
    if (mDestBuddy->ip() == "IP")
        setCurrentTransferBuddy(remoteDestinationAddress());
    else
        setCurrentTransferBuddy(mDestBuddy->username());

    // Preventive update of text send page
    setTextSnippetBuddy(mDestBuddy->username());
    setTextSnippetSending(true);
    setTextSnippet("");

    // Show send UI
    emit gotoSendPage();
    if (hasPendingShare() && ip != "IP") sendPendingShare();
}

void GuiBehind::sendDroppedFiles(const QStringList &files)
{
    if (files.isEmpty()) return;

    // Check if there's no selected buddy
    // (but there must be only one buddy in the buddy list)
    if (overlayState() == "")
    {
        if (mBuddiesList.rowCount() != 3) return;
        showSendPage(mBuddiesList.fistBuddyIp());
    }

    QStringList localPaths;

    foreach (const QString &url, files) {
        QUrl fileUrl(url);
        if (fileUrl.isLocalFile()) {
            QString path = fileUrl.toLocalFile();
            path.replace("\\", "/"); // Normalize to forward slashes
            localPaths.append(path);
        }
    }

    // Send files
    QStringList toSend = localPaths;
    startTransfer(toSend);
}

void GuiBehind::sendBuddyDroppedFiles(const QStringList &urls)
{
    if (urls.isEmpty()) return;

    QStringList localPaths;

    foreach (const QString &url, urls) {
        QUrl fileUrl(url);
        if (fileUrl.isLocalFile()) {
            QString path = fileUrl.toLocalFile();
            path.replace("\\", "/"); // Normalize to forward slashes
            localPaths.append(path);
        }
    }

    // Send files
    QStringList toSend = localPaths;
    startTransfer(toSend);
}

void GuiBehind::sendSomeFiles(const QStringList &files)
{
    QStringList sources = files;
#if defined(Q_OS_WIN)
    if (sources.isEmpty()) sources = QFileDialog::getOpenFileNames(nullptr, tr("Send some files"));
#endif
    if (!sources.isEmpty()) startTransfer(sources);
}

void GuiBehind::sendAllFiles(const QStringList &files)
{
    if (!files.isEmpty()) startTransfer(files);
}

void GuiBehind::sendClipboardText()
{
    // Get text to send
    QString text = mClipboard->text();
    if (text == "") return;
    // Other platforms can be re-added here if needed
    // Send text
    startTransfer(text);
}

void GuiBehind::sendText()
{
    // Get text to send
    QString text = textSnippet();
    if (text == "") return;

    // Send text
    startTransfer(text);
}

void GuiBehind::sendScreen()
{
    sendScreenStage2();
}

void GuiBehind::sendScreenStage2()
{
    if (mCapturingScreen) return;
    TransferRequest request;
    if (!captureRecipient(request)) return;
    mCapturingScreen = true;
    QList<QPointer<QWindow>> hidden;
    for (QWindow *window : QGuiApplication::allWindows()) {
        if (window->isVisible()) { hidden.append(window); window->hide(); }
    }
    QTimer::singleShot(300, this, [this, hidden, request]() mutable {
        auto *screen = QGuiApplication::primaryScreen();
        const QPixmap capture = screen ? screen->grabWindow(0) : QPixmap();
        for (const auto &window : hidden) if (window) window->show();
        mCapturingScreen = false;
        request.temporary = std::make_shared<QTemporaryDir>();
        const QString path = request.temporary->filePath("Screenshot.png");
        if (!request.temporary->isValid() || capture.isNull() || !capture.save(path, "PNG")) {
            showError(tr("Could not capture the desktop on this system."));
            return;
        }
        request.sources = {path};
        submit(std::move(request));
    });
}

void GuiBehind::startTransfer(QStringList files)
{
    if (files.isEmpty()) return;
    TransferRequest request;
    request.sources = files;
    if (captureRecipient(request)) submit(std::move(request));
}

void GuiBehind::startTransfer(QString text)
{
    if (text.isEmpty()) return;
    TransferRequest request;
    request.isText = true;
    request.text = text;
    if (captureRecipient(request)) submit(std::move(request));
}

bool GuiBehind::captureRecipient(TransferRequest &request)
{
    if (mDestBuddy->ip() == "IP") {
        const QString address = remoteDestinationAddress().trimmed();
        const QUrl url("tcp://" + address);
        if (address.isEmpty() || !url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()
            || !url.path().isEmpty() || url.hasQuery() || url.hasFragment()
            || url.port(4644) < 1 || url.port(4644) > 65535) {
            showError(tr("Enter a valid hostname or IP address, optionally followed by :port."));
            return false;
        }
        request.host = url.host();
        request.port = quint16(url.port(4644));
        request.recipient = address;
    } else {
        if (mDestBuddy->ip().isEmpty()) return false;
        request.host = mDestBuddy->ip();
        request.port = mDestBuddy->port() ? quint16(mDestBuddy->port()) : NETWORK_PORT;
        request.recipient = mDestBuddy->username();
    }
    return true;
}

void GuiBehind::submit(TransferRequest request)
{
    const bool queued = mTransfers.hasWork() || mDuktoProtocol->isBusy();
    mTransfers.enqueue(std::move(request));
    if (queued) {
        setMessagePageTitle(tr("Queue"));
        setMessagePageText(tr("Transfer queued. It will start automatically when the current transfer finishes.\n\nQueued items: %1").arg(mTransfers.queuedCount()));
        setMessagePageBackState("send");
        emit gotoMessagePage();
    }
}

void GuiBehind::startJob(TransferRequest request)
{
    mActiveJobId = request.id;
    mActiveCancellation = request.cancellation;
    mLastTransferError.clear();
    setCurrentTransferBuddy(request.recipient);
    setCurrentTransferSending(true);
    setCurrentTransferStats(tr("Connecting..."));
    setCurrentTransferProgress(0);
#if defined(Q_OS_ANDROID)
    setAndroidTransferActive(true);
#endif
    QMetaObject::invokeMethod(mDuktoProtocol, [this, request] { mDuktoProtocol->sendRequest(request); });
    emit transferStart();
}

void GuiBehind::sendFileComplete()
{
    mActiveJobId.clear();
    mActiveCancellation.reset();
    // Show completed message
    setMessagePageTitle(tr("Send"));
    setMessagePageText(tr("Your data has been sent to your buddy!\n\nDo you want to send other files to your buddy? Just drag and drop them here!"));
    setMessagePageBackState("send");
    // Other platforms can be re-added here if needed


#if defined(Q_OS_WIN)
    HWND hwnd = WinHelper::WinTaskbarProgressHelper::getMainWindowHandle();
    WinHelper::WinTaskbarProgressHelper::setSuccess(hwnd);
    if (!WinHelper::WinTaskbarProgressHelper::isMainWindowActive()) {
        WinHelper::WinTaskbarProgressHelper::flashWindowIfNotActive(hwnd);
        // Clear progress after a short delay to allow user to see the green bar
        QTimer::singleShot(1500, this, []() {
            WinHelper::WinTaskbarProgressHelper::clearProgress();
        });
    } else {
        // If app is focused, clear progress immediately
        WinHelper::WinTaskbarProgressHelper::clearProgress();
    }
#endif
    mTransfers.finish();
#if defined(Q_OS_ANDROID)
    if (!mTransfers.hasWork()) setAndroidTransferActive(false);
#endif
    emit gotoMessagePage();
}

void GuiBehind::sendFileError(int code)
{
    mActiveJobId.clear();
    mActiveCancellation.reset();
    setMessagePageTitle(tr("Error"));
    setMessagePageText(mLastTransferError.isEmpty() ? tr("An error occurred while sending your data. Error code: %1").arg(code) : mLastTransferError);
    setMessagePageBackState("send");
#if defined(Q_OS_WIN)
    HWND hwnd = WinHelper::WinTaskbarProgressHelper::getMainWindowHandle();
    WinHelper::WinTaskbarProgressHelper::setError(hwnd);
    if (!WinHelper::WinTaskbarProgressHelper::isMainWindowActive()) {
        WinHelper::WinTaskbarProgressHelper::flashWindowIfNotActive(hwnd);
        // Keep red progress until app is focused, then clear
        // Use an event filter to detect focus, or poll periodically
        static QMetaObject::Connection focusConn;
        if (focusConn)
            QObject::disconnect(focusConn);
        focusConn = QObject::connect(qApp, &QGuiApplication::applicationStateChanged, this, [hwnd](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive) {
                WinHelper::WinTaskbarProgressHelper::clearProgress();
                QObject::disconnect(focusConn);
                focusConn = {};
            }
        });
    } else {
        // If app is focused, clear progress after a short delay
        QTimer::singleShot(2000, this, []() {
            WinHelper::WinTaskbarProgressHelper::clearProgress();
        });
    }
#endif
    const bool retry = mTransfers.canRetry();
    mTransfers.finish(true);
#if defined(Q_OS_ANDROID)
    if (!mTransfers.hasWork()) setAndroidTransferActive(false);
#endif
    if (retry) setMessagePageText(messagePageText() + tr("\n\nThis transfer was re-queued for retry."));
    emit gotoMessagePage();
}

void GuiBehind::resetProgressStatus()
{
#if defined(Q_OS_WIN)
    WinHelper::WinTaskbarProgressHelper::clearProgress();
#endif
}

// Periodic hello sending
void GuiBehind::periodicHello()
{
#if defined(Q_OS_ANDROID)
    if (QGuiApplication::applicationState() != Qt::ApplicationActive) return;
#endif
    QMetaObject::invokeMethod(mDuktoProtocol, [this] { mDuktoProtocol->sayHello(QHostAddress::Broadcast); });
}

// Show updates message
void GuiBehind::showUpdatesMessage()
{
    setShowUpdateBanner(true);
}

// Abort current transfer while sending data
void GuiBehind::abortTransfer()
{
    if (!mTransfers.hasActive() && !mDuktoProtocol->isBusy()) return;
    const QString id = mActiveJobId;
    if (mActiveCancellation && !id.isEmpty()) mActiveCancellation->store(true);
    else mDuktoProtocol->requestCancel();
    QMetaObject::invokeMethod(mDuktoProtocol, [this, id] { mDuktoProtocol->abortRequest(id); });
}

void GuiBehind::acceptIncomingTransfer()
{
    if (mPendingIncomingSender.isEmpty())
        return;
    mPendingIncomingSender.clear();
    emit pendingIncomingSenderChanged();
    QMetaObject::invokeMethod(mDuktoProtocol, &DuktoProtocol::acceptIncomingTransfer);
}

void GuiBehind::rejectIncomingTransfer()
{
    if (mPendingIncomingSender.isEmpty())
        return;
    mPendingIncomingSender.clear();
    emit pendingIncomingSenderChanged();
    QMetaObject::invokeMethod(mDuktoProtocol, &DuktoProtocol::rejectIncomingTransfer);
    emit hideAllOverlays();
}

// Protocol confirms that abort has been done
void GuiBehind::sendFileAborted()
{
    mActiveJobId.clear();
    mActiveCancellation.reset();
    resetProgressStatus();
    mTransfers.finish();
#if defined(Q_OS_ANDROID)
    if (!mTransfers.hasWork()) setAndroidTransferActive(false);
#endif
    emit gotoSendPage();
}

// ------------------------------------------------------------
// Property setter and getter

QString GuiBehind::currentTransferBuddy()
{
    return mCurrentTransferBuddy;
}

void GuiBehind::setCurrentTransferBuddy(QString buddy)
{
    if (buddy == mCurrentTransferBuddy) return;
    mCurrentTransferBuddy = buddy;
    emit currentTransferBuddyChanged();
}

int GuiBehind::currentTransferProgress()
{
    return mCurrentTransferProgress;
}

void GuiBehind::setCurrentTransferProgress(int value)
{
    if (value == mCurrentTransferProgress) return;
    mCurrentTransferProgress = value;
    emit currentTransferProgressChanged();
#if defined(Q_OS_WIN)
    // Show progress on Windows taskbar
    WinHelper::WinTaskbarProgressHelper::setProgress(value, 100);
#endif
}

QString GuiBehind::currentTransferStats()
{
    return mCurrentTransferStats;
}

void GuiBehind::setCurrentTransferStats(QString stats)
{
    if (stats == mCurrentTransferStats) return;
    mCurrentTransferStats = stats;
    emit currentTransferStatsChanged();
}

QString GuiBehind::textSnippetBuddy()
{
    return mTextSnippetBuddy;
}

void GuiBehind::setTextSnippetBuddy(QString buddy)
{
    if (buddy == mTextSnippetBuddy) return;
    mTextSnippetBuddy = buddy;
    emit textSnippetBuddyChanged();
}

QString GuiBehind::textSnippet()
{
    return mTextSnippet;
}

void GuiBehind::setTextSnippet(QString text)
{
    if (text == mTextSnippet) return;
    mTextSnippet = text;
    emit textSnippetChanged();
}

bool GuiBehind::textSnippetSending()
{
    return mTextSnippetSending;
}

void GuiBehind::setTextSnippetSending(bool sending)
{
    if (sending == mTextSnippetSending) return;
    mTextSnippetSending = sending;
    emit textSnippetSendingChanged();
}

QString GuiBehind::currentPath()
{
    return mSettings.currentPath();
}

void GuiBehind::setCurrentPath(QString path)
{
    if (path == mSettings.currentPath()) return;
    mSettings.savePath(path);
    QMetaObject::invokeMethod(mDuktoProtocol, [this, path] { mDuktoProtocol->setReceivePath(path); });
    emit currentPathChanged();
}

bool GuiBehind::currentTransferSending()
{
    return mCurrentTransferSending;
}

void GuiBehind::setCurrentTransferSending(bool sending)
{
    if (sending == mCurrentTransferSending) return;
    mCurrentTransferSending = sending;
    emit currentTransferSendingChanged();
}

bool GuiBehind::clipboardTextAvailable()
{
    return mClipboardTextAvailable;
}

QString GuiBehind::remoteDestinationAddress()
{
    return mRemoteDestinationAddress;
}

void GuiBehind::setRemoteDestinationAddress(QString address)
{
    if (address == mRemoteDestinationAddress) return;
    mRemoteDestinationAddress = address;
    emit remoteDestinationAddressChanged();
}

QString GuiBehind::overlayState()
{
    return mOverlayState;
}

void GuiBehind::setOverlayState(QString state)
{
    if (state == mOverlayState) return;
    mOverlayState = state;
    emit overlayStateChanged();
}

QString GuiBehind::messagePageText()
{
    return mMessagePageText;
}

void GuiBehind::setMessagePageText(QString message)
{
    if (message == mMessagePageText) return;
    mMessagePageText = message;
    emit messagePageTextChanged();
}

QString GuiBehind::messagePageTitle()
{
    return mMessagePageTitle;
}

void GuiBehind::setMessagePageTitle(QString title)
{
    if (title == mMessagePageTitle) return;
    mMessagePageTitle = title;
    emit messagePageTitleChanged();
}

QString GuiBehind::messagePageBackState()
{
    return mMessagePageBackState;
}

void GuiBehind::setMessagePageBackState(QString state)
{
    if (state == mMessagePageBackState) return;
    mMessagePageBackState = state;
    emit messagePageBackStateChanged();
}

bool GuiBehind::showTermsOnStart()
{
    return mSettings.showTermsOnStart();
}

void GuiBehind::setShowTermsOnStart(bool show)
{
    mSettings.saveShowTermsOnStart(show);
    emit showTermsOnStartChanged();
}

bool GuiBehind::showUpdateBanner()
{
    return mShowUpdateBanner;
}

void GuiBehind::setShowUpdateBanner(bool show)
{
    mShowUpdateBanner = show;
    emit showUpdateBannerChanged();
}

bool GuiBehind::confirmBeforeReceive()
{
    return mSettings.confirmBeforeReceive();
}

void GuiBehind::setConfirmBeforeReceive(bool enabled)
{
    mSettings.saveConfirmBeforeReceive(enabled);
    emit confirmBeforeReceiveChanged();
}

QString GuiBehind::pendingIncomingSender()
{
    return mPendingIncomingSender;
}

void GuiBehind::setBuddyName(QString name)
{
    qDebug() << "Buddy name is:  " << name;
    mSettings.saveBuddyName(name.replace(' ', ""));
    QMetaObject::invokeMethod(mDuktoProtocol, &DuktoProtocol::updateBuddyName);
    mBuddiesList.updateMeElement();
    emit buddyNameChanged();
}

QString GuiBehind::buddyName()
{
    return mSettings.buddyName();
}

QString GuiBehind::appVersion()
{
    return QCoreApplication::applicationVersion();
}

bool GuiBehind::isTrayIconVisible()
{
#if defined(Q_OS_WIN)
    if (QSystemTrayIcon::isSystemTrayAvailable() && trayIcon)
        return trayIcon->isVisible();
#endif
    return false;
}

void GuiBehind::setTrayIconVisible(bool bVisible)
{
#if defined(Q_OS_WIN)
    if (QSystemTrayIcon::isSystemTrayAvailable() && trayIcon)
        trayIcon->setVisible(bVisible);
#endif
}

#if defined(Q_OS_WIN)
void GuiBehind::createActions()
{
    minimizeAction = new QAction(tr("Mi&nimize"), this);
    connect(minimizeAction, &QAction::triggered, this, []() {
        QWidgetList topLevels = QApplication::topLevelWidgets();
        for (int i = 0; i < topLevels.size(); ++i) {
            QWidget *w = topLevels.at(i);
            if (w->isWindow() && w->isVisible()) {
                w->showMinimized();
                break;
            }
        }
    });

    restoreAction = new QAction(tr("&Restore"), this);
    connect(restoreAction, &QAction::triggered, this, []() {
        QWidgetList topLevels = QApplication::topLevelWidgets();
        for (int i = 0; i < topLevels.size(); ++i) {
            QWidget *w = topLevels.at(i);
            if (w->isWindow()) {
                w->showNormal();
                break;
            }
        }
    });

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}

void GuiBehind::createTrayIcon()
{
    trayIconMenu = new QMenu();
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    QIcon icon(":/src/assets/dukto.png"); // Ensure this path matches your resource
    trayIcon->setIcon(icon); // Set icon before showing
    connect(trayIcon, &QSystemTrayIcon::activated, this, &GuiBehind::iconActivated);
}
#endif

#if defined(Q_OS_WIN)
void GuiBehind::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // For QtQuick/QML, get the first visible window and restore it
        const auto windows = QGuiApplication::allWindows();
        for (QWindow *w : windows) {
            if (w && w->isVisible()) {
                if (w->windowState() & Qt::WindowMinimized) {
                    w->setWindowState(Qt::WindowNoState);
                }
                w->show();
                w->raise();
                w->requestActivate();
                break;
            }
        }
    }
}
#endif

// Add this if missing
void GuiBehind::remoteDestinationAddressHandler()
{
    // Update GUI status
    setCurrentTransferBuddy(remoteDestinationAddress());
    setTextSnippetBuddy(remoteDestinationAddress());
}

// Add this if missing
bool GuiBehind::canAcceptDrop()
{
    // There must be the send page shown and,
    // if it's a remote destination, it must have an IP
    if (overlayState() == "send")
        return !((mDestBuddy->ip() == "IP") && (remoteDestinationAddress() == ""));

    // Or there could be a "send complete" or "send error" message relative to a
    // determinate buddy
    else if ((overlayState() == "message") && (messagePageBackState() == "send"))
        return true;

    // Or there could be just one buddy in the list
    else if (mBuddiesList.rowCount() == 3)
        return true;

    return false;
}

void GuiBehind::close()
{
    QMetaObject::invokeMethod(mDuktoProtocol, [this] { mDuktoProtocol->sayGoodbye(); });
}

void GuiBehind::changeThemeColor(QString color)
{
    mTheme.setThemeColor(color);
    mSettings.saveThemeColor(color);
}

void GuiBehind::setThemeMode(const QString &mode)
{
    if (mTheme.mode() == mode) return;
    mTheme.setMode(mode);
    if (mTheme.mode() != mode) return;
    mSettings.saveThemeMode(mode);
    emit themeModeChanged();
}

void GuiBehind::receiveFileCancelled()
{
    mTransfers.setReceiving(false);
    dismissIncomingTransfer();
    // You can add error handling or user notification here if needed.
    // For now, just reset the progress status.
#if defined(Q_OS_WIN)
    HWND hwnd = WinHelper::WinTaskbarProgressHelper::getMainWindowHandle();
    WinHelper::WinTaskbarProgressHelper::setError(hwnd);
    if (!WinHelper::WinTaskbarProgressHelper::isMainWindowActive()) {
        WinHelper::WinTaskbarProgressHelper::flashWindowIfNotActive(hwnd);
    }
    QTimer::singleShot(2000, this, []() {
        WinHelper::WinTaskbarProgressHelper::clearProgress();
    });
#else
    resetProgressStatus();
#endif
#if defined(Q_OS_ANDROID)
    if (!mTransfers.hasWork()) setAndroidTransferActive(false);
#endif
    showError(mLastTransferError.isEmpty() ? tr("The incoming transfer was interrupted.") : mLastTransferError);
    setMessagePageBackState("");
}

GuiBehind::~GuiBehind()
{
    shutdown();
}

void GuiBehind::shutdown()
{
    if (mShuttingDown) return;
    mShuttingDown = true;
    mTransfers.setPaused(true);
    if (mDuktoProtocol && mProtocolThread.isRunning()) {
        mDuktoProtocol->requestCancel();
        QMetaObject::invokeMethod(mDuktoProtocol, [this] {
            mDuktoProtocol->abortCurrentTransfer();
            mDuktoProtocol->sayGoodbye();
        }, Qt::BlockingQueuedConnection);
        mProtocolThread.quit();
        mProtocolThread.wait();
    }
    mTransfers.clear();
#if defined(Q_OS_ANDROID)
    setAndroidTransferActive(false);
#endif
}

void GuiBehind::readyForShares()
{
    mAndroidShares.ready();
}

void GuiBehind::stageShare(QStringList sources, QString text)
{
    TransferRequest share;
    share.sources = sources;
    share.isText = sources.isEmpty();
    share.text = sources.isEmpty() ? text : QString();
    if (share.sources.isEmpty() && share.text.isEmpty()) return;
    mPendingShares.enqueue(std::move(share));
    emit pendingShareChanged();
    if (!mTransfers.hasActive() && !mDuktoProtocol->isBusy()) {
        setMessagePageTitle(tr("Shared content"));
        setMessagePageText(tr("Your shared content is ready. Go back and choose a buddy to send it to. For an IP connection, enter the address and choose Send shared content."));
        setMessagePageBackState("");
        emit gotoMessagePage();
    }
}

void GuiBehind::sendPendingShare()
{
    if (mPendingShares.isEmpty()) return;
    TransferRequest request = mPendingShares.head();
    if (!captureRecipient(request)) return;
    mPendingShares.dequeue();
    emit pendingShareChanged();
    submit(std::move(request));
}

void GuiBehind::dismissIncomingTransfer()
{
    mPendingIncomingSender.clear();
    emit pendingIncomingSenderChanged();
    if (overlayState() == "confirmreceive") emit hideAllOverlays();
    mTransfers.setReceiving(false);
#if defined(Q_OS_ANDROID)
    if (!mTransfers.hasWork()) setAndroidTransferActive(false);
#endif
}

void GuiBehind::showError(const QString &message)
{
    setMessagePageTitle(tr("Error"));
    setMessagePageText(message);
    setMessagePageBackState("send");
    emit gotoMessagePage();
}

QString GuiBehind::androidStorageDescription() const
{
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    return QJniObject::callStaticObjectMethod("idv/coolshou/DuktoStorage", "description",
        "(Landroid/content/Context;)Ljava/lang/String;", context.object<jobject>()).toString();
#else
    return {};
#endif
}

bool GuiBehind::androidCustomStorage() const
{
    return !ReceivedFile::selectedTree().isEmpty();
}

bool GuiBehind::androidAutomaticStorageAvailable() const
{
#ifdef Q_OS_ANDROID
    return QNativeInterface::QAndroidApplication::sdkVersion() >= 29;
#else
    return false;
#endif
}

void GuiBehind::chooseAndroidStorage()
{
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoActivity", "chooseReceiveFolder",
        "(Landroid/content/Context;)V", context.object<jobject>());
#endif
}

void GuiBehind::useAutomaticAndroidStorage()
{
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    const auto empty = QJniObject::fromString("");
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoStorage", "selectTree",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V",
        context.object<jobject>(), empty.object<jstring>(), empty.object<jstring>());
    emit androidStorageChanged();
#endif
}

void GuiBehind::openBatterySettings()
{
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoBridge", "openBatterySettings",
        "(Landroid/content/Context;)V", context.object<jobject>());
#endif
}
