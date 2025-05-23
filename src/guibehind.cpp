#include "guibehind.h"
#include "platform.h"

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
#include <QTemporaryFile>
#include <QTimer>
#include <QClipboard>
#include <QFileDialog>
#include <QApplication>
#include <QScreen>
#if defined(Q_OS_WIN)
#include <QWindow>
#include <windows.h>
#include <ShObjIdl.h>

// Helper for Windows 7+ taskbar progress
class WinTaskbarProgressHelper {
public:
    static ITaskbarList3* getTaskbar() {
        static ITaskbarList3* pTaskbar = nullptr;
        static bool initialized = false;
        if (!initialized) {
            CoInitialize(nullptr);
            HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_ITaskbarList3, (void**)&pTaskbar);
            initialized = true;
        }
        return pTaskbar;
    }
    static HWND getMainWindowHandle() {
        // Try to get the first visible top-level window
        const auto windows = QGuiApplication::allWindows();
        for (QWindow *w : windows) {
            if (w && w->isVisible()) {
                return (HWND)w->winId();
            }
        }
        return nullptr;
    }
    static void setProgress(int value, int maximum) {
        ITaskbarList3* pTaskbar = getTaskbar();
        HWND hwnd = getMainWindowHandle();
        if (pTaskbar && hwnd) {
            if (maximum > 0 && value >= 0) {
                pTaskbar->SetProgressState(hwnd, TBPF_NORMAL);
                pTaskbar->SetProgressValue(hwnd, value, maximum);
            } else {
                pTaskbar->SetProgressState(hwnd, TBPF_NOPROGRESS);
            }
        }
    }
    static void clearProgress() {
        ITaskbarList3* pTaskbar = getTaskbar();
        HWND hwnd = getMainWindowHandle();
        if (pTaskbar && hwnd) {
            pTaskbar->SetProgressState(hwnd, TBPF_NOPROGRESS);
        }
    }
    static void cleanup() {
        ITaskbarList3* pTaskbar = getTaskbar();
        if (pTaskbar) {
            pTaskbar->Release();
            CoUninitialize();
        }
    }
};
#endif

#include <QGuiApplication>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <QJniEnvironment>
#include <QtCore/private/qandroidextras_p.h>
#include <QStandardPaths>
#include <QDir>
#include <QList>
#include <QString>
#endif

#define NETWORK_PORT 4644 // 6742

#if defined(Q_OS_ANDROID)
#endif

// The constructor is private and can only be called within the singleton instance method
GuiBehind::GuiBehind(QQmlApplicationEngine &engine, QObject *parent) :
    QObject(parent), mShowBackTimer(NULL), mPeriodicHelloTimer(NULL), mClipboard(NULL),
    mMiniWebServer(NULL), mSettings(this), mDestBuddy(NULL), mUpdatesChecker(NULL)
{
#if defined(Q_OS_ANDROID)
    requestPermissions();
#endif
    // Other platforms can be re-added here if needed

    // Change current folder
    QDir::setCurrent(currentPath());

    // QString rootPath = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).value(0);
    // qDebug() << "The general download path is:" << rootPath;

    // Status variables
    // mView->setGuiBehindReference(this);
    setCurrentTransferProgress(0);
    setTextSnippetSending(false);
    setShowUpdateBanner(false);

    // Clipboard object
    mClipboard = QApplication::clipboard();
    connect(mClipboard, SIGNAL(dataChanged()), this, SLOT(clipboardChanged()));
    clipboardChanged(); // Initial call to handle current clipboard data

    // Add "Me" entry
    mBuddiesList.addMeElement();

    // Add "Ip" entry
    mBuddiesList.addIpElement();

    // Destination buddy
    mDestBuddy = new DestinationBuddy(this);

    // Mini web server
    mMiniWebServer = new MiniWebServer(NETWORK_PORT + 1);

    // Initialize and set the current theme color
    mTheme.setThemeColor(mSettings.themeColor());

    // Expose instances to QML context
    engine.rootContext()->setContextProperty("buddiesListData", &mBuddiesList);
    engine.rootContext()->setContextProperty("recentListData", &mRecentList);
    engine.rootContext()->setContextProperty("ipAddressesData", &mIpAddresses);
    engine.rootContext()->setContextProperty("guiBehind", this);
    engine.rootContext()->setContextProperty("destinationBuddy", mDestBuddy);
    engine.rootContext()->setContextProperty("theme", &mTheme);

    // Register protocol signals
    connect(&mDuktoProtocol, SIGNAL(peerListAdded(Peer)), this, SLOT(peerListAdded(Peer)));
    connect(&mDuktoProtocol, SIGNAL(peerListRemoved(Peer)), this, SLOT(peerListRemoved(Peer)));
    connect(&mDuktoProtocol, SIGNAL(receiveFileStart(QString)), this, SLOT(receiveFileStart(QString)));
    connect(&mDuktoProtocol, SIGNAL(transferStatusUpdate(qint64,qint64)), this, SLOT(transferStatusUpdate(qint64,qint64)));
    connect(&mDuktoProtocol, SIGNAL(receiveFileComplete(QStringList*,qint64)), this, SLOT(receiveFileComplete(QStringList*,qint64)));
    connect(&mDuktoProtocol, SIGNAL(receiveTextComplete(QString*,qint64)), this, SLOT(receiveTextComplete(QString*,qint64)));
    connect(&mDuktoProtocol, SIGNAL(sendFileComplete()), this, SLOT(sendFileComplete()));
    connect(&mDuktoProtocol, SIGNAL(sendFileError(int)), this, SLOT(sendFileError(int)));
    connect(&mDuktoProtocol, SIGNAL(receiveFileCancelled()), this, SLOT(receiveFileCancelled()));
    connect(&mDuktoProtocol, SIGNAL(sendFileAborted()), this, SLOT(sendFileAborted()));

    // Register other signals
    connect(this, SIGNAL(remoteDestinationAddressChanged()), this, SLOT(remoteDestinationAddressHandler()));

    // Say "hello"
    mDuktoProtocol.setPorts(NETWORK_PORT, NETWORK_PORT);
    mDuktoProtocol.initialize();
    mDuktoProtocol.sayHello(QHostAddress::Broadcast);

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
    mUpdatesChecker = new UpdatesChecker();
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
}

#if defined(Q_OS_ANDROID)
// Request Permissions on Android
bool GuiBehind::requestPermissions() {
    QList<bool> permissions;

    // Declare required permissions based on Android version
    const QStringList permissionList = {
        // For Android 12 and below
        "android.permission.WRITE_EXTERNAL_STORAGE",
        "android.permission.READ_EXTERNAL_STORAGE",
        // For Android 13+ (API 33+)
        "android.permission.READ_MEDIA_IMAGES",
        "android.permission.READ_MEDIA_VIDEO",
        "android.permission.READ_MEDIA_AUDIO",
        // Optional
        "android.permission.WRITE_SETTINGS",
        "android.permission.WAKE_LOCK"
    };

    // TODO: Optionally check Android version and only request relevant permissions

    for (const QString &permission : permissionList) {
        auto result = QtAndroidPrivate::checkPermission(permission).result();
        if (result != QtAndroidPrivate::Authorized) {
            result = QtAndroidPrivate::requestPermission(permission).result();
            if (result == QtAndroidPrivate::Denied) {
                permissions.append(false);
            }
        }
    }

    return permissions.isEmpty();
}

// NOTE: For content URIs, you cannot use QFile directly. You must use JNI to obtain an InputStream or file descriptor.
// See convertContentUriToFilePath or implement a method to open a stream from URI using JNI and pass it to Qt.
#endif

QRect GuiBehind::windowGeometry()
{
    return mSettings.windowRect();
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
    // Look for the sender in the buddy list
    QString sender = mBuddiesList.buddyNameByIp(senderIp);
    if (sender == "")
        setCurrentTransferBuddy("remote sender");
    else
        setCurrentTransferBuddy(sender);

    // Update user interface
    setCurrentTransferSending(false);
    // mView->win7()->setProgressValue(0, 100);
    // mView->win7()->setProgressState(EcWin7::Normal);

    emit transferStart();
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

    double percent = partial * 1.0 / total * 100;
    setCurrentTransferProgress(percent);

#if defined(Q_OS_WIN)
    // Qt 6.9+: QWinTaskbarButton/Progress is removed, so do nothing here.
    // Optionally, you can implement Windows 10 taskbar progress using native Win32 API if needed.
    // For now, just leave this block empty.
#endif
    // mView->win7()->setProgressValue(percent, 100);
}

void GuiBehind::receiveFileComplete(QStringList *files, qint64 totalSize) {

    // Add an entry to recent activities
    QDir d(".");
    if (files->size() == 1)
        mRecentList.addRecent(files->at(0), d.absoluteFilePath(files->at(0)), "file", mCurrentTransferBuddy, totalSize);
    else
        mRecentList.addRecent(tr("Files and folders"), d.absolutePath(), "misc", mCurrentTransferBuddy, totalSize);

// Update GUI
// mView->win7()->setProgressState(EcWin7::NoProgress);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    //TODO: check QtQuick 2 alert
    QApplication::alert(mView, 5000);
#endif
    emit receiveCompleted();
}

void GuiBehind::receiveTextComplete(QString *text, qint64 totalSize)
{
    // Add an entry to recent activities
    mRecentList.addRecent(tr("Text snippet"), *text, "text", mCurrentTransferBuddy, totalSize);

// Update GUI
// mView->win7()->setProgressState(EcWin7::NoProgress);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    //TODO: check QtQuick 2 alert
    QApplication::alert(mView, 5000);
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
    // Use Android Intent to open file
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) return;

    QJniObject filePath = QJniObject::fromString(path);
    QJniObject fileObj("java/io/File", "(Ljava/lang/String;)V", filePath.object<jstring>());
    if (!fileObj.isValid()) return;

    QJniObject uri = QJniObject::callStaticObjectMethod(
        "androidx/core/content/FileProvider",
        "getUriForFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
        context.object<jobject>(),
        QJniObject::fromString(context.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString() + ".qtprovider").object<jstring>(),
        fileObj.object<jobject>()
    );
    if (!uri.isValid()) return;

    // Get MIME type
    QJniObject contentResolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    QJniObject mimeType = contentResolver.callObjectMethod(
        "getType",
        "(Landroid/net/Uri;)Ljava/lang/String;",
        uri.object<jobject>()
    );
    QString mime = mimeType.isValid() ? mimeType.toString() : "application/octet-stream";

    // Create intent
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      QJniObject::fromString("android.intent.action.VIEW").object<jstring>());
    intent.callObjectMethod("setDataAndType",
                            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;",
                            uri.object<jobject>(),
                            QJniObject::fromString(mime).object<jstring>());
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", 1 /* FLAG_GRANT_READ_URI_PERMISSION */);

    // Start activity
    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object<jobject>());
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
}

void GuiBehind::openDestinationFolder() {
#if defined(Q_OS_ANDROID)
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath()));
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
            localPaths.append(fileUrl.toLocalFile());
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
            localPaths.append(fileUrl.toLocalFile());
        }
    }

    // qDebug() << localPaths;

    // Send files
    QStringList toSend = localPaths;
    startTransfer(toSend);
}

void GuiBehind::sendSomeFiles(const QStringList &files)
{
#if defined(Q_OS_WIN)
    QStringList localPaths = files;
    if (localPaths.isEmpty()) {
        localPaths = QFileDialog::getOpenFileNames(nullptr, tr("Send some files"));
        if (localPaths.isEmpty()) return;
    }
    startTransfer(localPaths);
#else
    if (files.isEmpty()) return;
    QStringList localPaths;
#if defined(Q_OS_ANDROID)
    mTempFilesForTransfer.clear();
#endif

    foreach (const QString &file, files) {
#if defined(Q_OS_ANDROID)
        QUrl fileUrl(file);
        if (fileUrl.scheme() == "content") {
            // Copy content URI to temp file
            QString tempFilePath = copyContentUriToTempFile(file);
            if (!tempFilePath.isEmpty()) {
                localPaths.append(tempFilePath);
                mTempFilesForTransfer.append(tempFilePath);
            }
            continue;
        }
#endif
        // Only define fileUrl if not already defined above
#if !defined(Q_OS_ANDROID)
        QUrl fileUrl(file);
#endif
        if (fileUrl.isLocalFile()) {
            localPaths.append(fileUrl.toLocalFile());
        }
    }

    if (localPaths.isEmpty()) return;

    // Send files
    startTransfer(localPaths);
#endif
}

// JNI helper: copy content URI to temp file and return its path
#if defined(Q_OS_ANDROID)
QString GuiBehind::copyContentUriToTempFile(const QString &uri) {
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) return QString();

    QJniObject jUriString = QJniObject::fromString(uri);
    QJniObject uriObj = QJniObject::callStaticObjectMethod(
        "android/net/Uri",
        "parse",
        "(Ljava/lang/String;)Landroid/net/Uri;",
        jUriString.object<jstring>()
    );
    if (!uriObj.isValid()) return QString();

    // Query original filename from content resolver
    QString displayName = "file";
    QJniObject contentResolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    if (contentResolver.isValid()) {
        QJniEnvironment env;
        jclass stringClass = env->FindClass("java/lang/String");
        jobjectArray projArray = env->NewObjectArray(1, stringClass, nullptr);
        QJniObject colName = QJniObject::fromString("_display_name");
        env->SetObjectArrayElement(projArray, 0, colName.object<jobject>());
        QJniObject cursor = contentResolver.callObjectMethod(
            "query",
            "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
            uriObj.object<jobject>(),
            projArray,
            nullptr,
            nullptr,
            nullptr
        );
        if (cursor.isValid()) {
            jobject jCursor = cursor.object<jobject>();
            jclass cursorClass = env->GetObjectClass(jCursor);
            jmethodID moveToFirst = env->GetMethodID(cursorClass, "moveToFirst", "()Z");
            if (env->CallBooleanMethod(jCursor, moveToFirst)) {
                jmethodID getColumnIndex = env->GetMethodID(cursorClass, "getColumnIndex", "(Ljava/lang/String;)I");
                jint colIdx = env->CallIntMethod(jCursor, getColumnIndex, colName.object<jstring>());
                jmethodID getString = env->GetMethodID(cursorClass, "getString", "(I)Ljava/lang/String;");
                jstring jDisplayName = (jstring)env->CallObjectMethod(jCursor, getString, colIdx);
                if (jDisplayName) {
                    const char *cstr = env->GetStringUTFChars(jDisplayName, nullptr);
                    displayName = QString::fromUtf8(cstr);
                    env->ReleaseStringUTFChars(jDisplayName, cstr);
                    env->DeleteLocalRef(jDisplayName);
                }
            }
            // Close cursor
            jmethodID closeCursor = env->GetMethodID(cursorClass, "close", "()V");
            env->CallVoidMethod(jCursor, closeCursor);
            env->DeleteLocalRef(cursorClass);
        }
        env->DeleteLocalRef(projArray);
        env->DeleteLocalRef(stringClass);
    }

    // Fallback if displayName is empty
    if (displayName.isEmpty()) {
        displayName = QString::number(QDateTime::currentMSecsSinceEpoch());
    }

    // Create temp file path with original filename
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/" + displayName;
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly)) return QString();

    // Read InputStream and write to temp file
    QJniEnvironment env;
    QJniObject inputStream = contentResolver.callObjectMethod(
        "openInputStream",
        "(Landroid/net/Uri;)Ljava/io/InputStream;",
        uriObj.object<jobject>()
    );
    if (!inputStream.isValid()) return QString();

    jbyteArray buffer = env->NewByteArray(4096);
    jint bytesRead = 0;
    jclass inputStreamClass = env->GetObjectClass(inputStream.object<jobject>());
    jmethodID readMethod = env->GetMethodID(inputStreamClass, "read", "([B)I");

    while (true) {
        bytesRead = env->CallIntMethod(inputStream.object<jobject>(), readMethod, buffer);
        if (bytesRead <= 0) break;
        jbyte* bufPtr = env->GetByteArrayElements(buffer, nullptr);
        tempFile.write(reinterpret_cast<const char*>(bufPtr), bytesRead);
        env->ReleaseByteArrayElements(buffer, bufPtr, JNI_ABORT);
    }
    tempFile.close();

    // Close InputStream
    jmethodID closeMethod = env->GetMethodID(inputStreamClass, "close", "()V");
    env->CallVoidMethod(inputStream.object<jobject>(), closeMethod);

    env->DeleteLocalRef(buffer);
    env->DeleteLocalRef(inputStreamClass);

    return tempPath;
}
#endif

void GuiBehind::sendAllFiles(const QStringList &files)
{
    if (files.isEmpty()) return;

    QStringList localPaths;

    foreach (const QString &file, files) {
        QUrl fileUrl(file);
        if (fileUrl.isLocalFile()) {
            localPaths.append(fileUrl.toLocalFile());
        }
    }

    // qDebug() << localPaths;

    // Send files
    QStringList toSend = localPaths;
    startTransfer(toSend);
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
#if defined(Q_OS_WIN)
    // Minimize window if possible (QWidget-based main window)
    QWidgetList topLevels = QApplication::topLevelWidgets();
    for (QWidget *w : topLevels) {
        if (w->isWindow() && w->isVisible()) {
            w->setWindowState(Qt::WindowMinimized);
            break;
        }
    }
#endif
    QTimer::singleShot(500, this, SLOT(sendScreenStage2()));
}

void GuiBehind::sendScreenStage2()
{
#if defined(Q_OS_WIN)
    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow(0);
    // Restore window if possible
    QWidgetList topLevels = QApplication::topLevelWidgets();
    for (QWidget *w : topLevels) {
        if (w->isWindow() && w->windowState() == Qt::WindowMinimized) {
            w->setWindowState(Qt::WindowActive);
            break;
        }
    }
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    tempFile.open();
    mScreenTempPath = tempFile.fileName();
    tempFile.close();
    pixmap.save(mScreenTempPath, "JPG", 95);
    QString ip;
    qint16 port;
    if (!prepareStartTransfer(&ip, &port)) return;
    mDuktoProtocol.sendScreen(ip, port, mScreenTempPath);
#else
    // Screenshot
    // QPixmap screen = QPixmap::grabWindow(QApplication::desktop()->winId());
    QPixmap screen = QGuiApplication::primaryScreen()->grabWindow(0);
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    tempFile.open();
    mScreenTempPath = tempFile.fileName();
    tempFile.close();
    screen.save(mScreenTempPath, "JPG", 95);
    QString ip;
    qint16 port;
    if (!prepareStartTransfer(&ip, &port)) return;
    mDuktoProtocol.sendScreen(ip, port, mScreenTempPath);
#endif
}

void GuiBehind::startTransfer(QStringList files)
{
    // Prepare file transfer
    QString ip;
    qint16 port;
    if (!prepareStartTransfer(&ip, &port)) return;

    // Start files transfer
    mDuktoProtocol.sendFile(ip, port, files);
}

void GuiBehind::startTransfer(QString text)
{
    // Prepare file transfer
    QString ip;
    qint16 port;
    if (!prepareStartTransfer(&ip, &port)) return;

    // Start files transfer
    mDuktoProtocol.sendText(ip, port, text);
}

bool GuiBehind::prepareStartTransfer(QString *ip, qint16 *port)
{
    // Check if it's a remote file transfer
    if (mDestBuddy->ip() == "IP") {

        // Remote transfer
        QString dest = remoteDestinationAddress();

        // Check if port is specified
        if (dest.contains(":")) {

            // Port is specified or destination is malformed...
            static const QRegularExpression rx("^(.*):([0-9]+)$");
            QRegularExpressionMatch match = rx.match(dest);
            if (!match.hasMatch()) {

                // Malformed destination
                setMessagePageTitle(tr("Send"));
                setMessagePageText(tr("Hey, take a look at your destination, it appears to be malformed!"));
                setMessagePageBackState("send");
                emit gotoMessagePage();
                return false;
            }

            // Get IP (or hostname) and port
            *ip = match.captured(1);
            *port = match.captured(2).toInt();
        }
        else {

            // Port not specified, using default
            *ip = dest;
            *port = 0;
        }
        setCurrentTransferBuddy(*ip);
    }
    else {

        // Local transfer
        *ip = mDestBuddy->ip();
        *port = mDestBuddy->port();
        setCurrentTransferBuddy(mDestBuddy->username());
    }

    // Update GUI for file transfer
    setCurrentTransferSending(true);
    setCurrentTransferStats(tr("Connecting..."));
    setCurrentTransferProgress(0);
    // mView->win7()->setProgressState(EcWin7::Normal);
    // mView->win7()->setProgressValue(0, 100);

    emit transferStart();
    return true;
}

void GuiBehind::sendFileComplete()
{
    // Show completed message
    setMessagePageTitle(tr("Send"));
    setMessagePageText(tr("Your data has been sent to your buddy!\n\nDo you want to send other files to your buddy? Just drag and drop them here!"));
    setMessagePageBackState("send");
    // Other platforms can be re-added here if needed
    // mView->win7()->setProgressState(EcWin7::NoProgress);

    // Check for temporary file to delete
    if (mScreenTempPath != "") {

        QFile file(mScreenTempPath);
        file.remove();
        mScreenTempPath = "";
    }

#if defined(Q_OS_ANDROID)
    // Clean up temp files after sending
    for (const QString &tempPath : mTempFilesForTransfer) {
        QFile::remove(tempPath);
    }
    mTempFilesForTransfer.clear();
#endif
#if defined(Q_OS_WIN)
    // Qt 6.9+: QWinTaskbarButton/Progress is removed, so do nothing here.
#endif
    emit gotoMessagePage();
}

void GuiBehind::sendFileError(int code)
{
    setMessagePageTitle(tr("Error"));
    setMessagePageText(tr("Sorry, an error has occurred while sending your data...\n\nError code: ") + QString::number(code));
    setMessagePageBackState("send");
#if defined(Q_OS_ANDROID)
    // Clean up temp files on error
    for (const QString &tempPath : mTempFilesForTransfer) {
        QFile::remove(tempPath);
    }
    mTempFilesForTransfer.clear();
#endif
    // Check for temporary file to delete
    if (mScreenTempPath != "") {
        QFile file(mScreenTempPath);
        file.remove();
        mScreenTempPath = "";
    }
#if defined(Q_OS_WIN)
    // Qt 6.9+: QWinTaskbarButton/Progress is removed, so do nothing here.
#endif
    emit gotoMessagePage();
}

void GuiBehind::receiveFileCancelled()
{
    setMessagePageTitle(tr("Error"));
    setMessagePageText(tr("An error has occurred during the transfer... The data you received could be incomplete or broken."));
    setMessagePageBackState("");
    emit gotoMessagePage();
#if defined(Q_OS_WIN)
    // Qt 6.9+: QWinTaskbarButton/Progress is removed, so do nothing here.
#endif
}

void GuiBehind::resetProgressStatus()
{
#if defined(Q_OS_WIN)
    // Clear progress on Windows taskbar
    WinTaskbarProgressHelper::clearProgress();
#endif
}

// Periodic hello sending
void GuiBehind::periodicHello()
{
    mDuktoProtocol.sayHello(QHostAddress::Broadcast);
}

// Show updates message
void GuiBehind::showUpdatesMessage()
{
    setShowUpdateBanner(true);
}

// Abort current transfer while sending data
void GuiBehind::abortTransfer()
{
    mDuktoProtocol.abortCurrentTransfer();
}

// Protocol confirms that abort has been done
void GuiBehind::sendFileAborted()
{
    resetProgressStatus();
#if defined(Q_OS_ANDROID)
    // Clean up temp files on abort
    for (const QString &tempPath : mTempFilesForTransfer) {
        QFile::remove(tempPath);
    }
    mTempFilesForTransfer.clear();
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
    WinTaskbarProgressHelper::setProgress(value, 100);
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

void GuiBehind::setBuddyName(QString name)
{
    qDebug() << "Buddy name is:  " << name;
    mSettings.saveBuddyName(name.replace(' ', ""));
    mDuktoProtocol.updateBuddyName();
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
#if defined(Q_OS_UNIX) || defined(Q_OS_ANDROID)
    if (QSystemTrayIcon::isSystemTrayAvailable() && trayIcon)
        return trayIcon->isVisible();
#endif
    return false;
}

void GuiBehind::setTrayIconVisible(bool bVisible)
{
#if defined(Q_OS_UNIX) || defined(Q_OS_ANDROID)
    if (QSystemTrayIcon::isSystemTrayAvailable() && trayIcon)
        trayIcon->setVisible(bVisible);
#endif
}

void GuiBehind::createActions()
{
#if defined(Q_OS_WIN)
    minimizeAction = new QAction(tr("Mi&nimize"), this);
    connect(minimizeAction, &QAction::triggered, []() {
        QWidgetList topLevels = QApplication::topLevelWidgets();
        for (QWidget *w : topLevels) {
            if (w->isWindow() && w->isVisible()) {
                w->showMinimized();
                break;
            }
        }
    });

    restoreAction = new QAction(tr("&Restore"), this);
    connect(restoreAction, &QAction::triggered, []() {
        QWidgetList topLevels = QApplication::topLevelWidgets();
        for (QWidget *w : topLevels) {
            if (w->isWindow()) {
                w->showNormal();
                break;
            }
        }
    });

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
#endif
}

void GuiBehind::createTrayIcon()
{
#if defined(Q_OS_WIN)
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
#endif
}

void GuiBehind::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
#if defined(Q_OS_WIN)
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
#endif
    // ...existing code...
}

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
    mDuktoProtocol.sayGoodbye();
}

void GuiBehind::changeThemeColor(QString color)
{
    mTheme.setThemeColor(color);
    mSettings.saveThemeColor(color);
}
