#include "platform.h"

#include <QString>
#include <QHostInfo>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#if defined(Q_OS_MAC)
#include <QTemporaryFile>
#include <CoreServices/CoreServices.h>
#endif

#if defined(Q_OS_WIN)
#include <windows.h>
#include <lmaccess.h>
#include <QMessageBox>
#endif

#if defined(Q_OS_S60)
#define SYMBIAN
#include <QSystemDeviceInfo>
QTM_USE_NAMESPACE
#endif

#if defined(Q_OS_SIMULATOR)
#define SYMBIAN
#endif

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

#include "settings.h"
    QString Platform::username = "";

// Returns the system username
QString Platform::getSystemUsername()
{
    Settings s;
    qDebug() << "buddyName:" << s.buddyName();
    username = !s.buddyName().isEmpty() ? s.buddyName() : "";
    if (username != "") return username;

#if defined(Q_OS_ANDROID) || defined(Q_OS_UNIX)
    QString uname = qgetenv("USER");  // for Linux/Android
    if (uname.isEmpty())
        uname = "Unknown";
    return uname;
#else
    // Other platforms can be re-added here
    return "Unknown";
#endif
}

void Platform::updateUsername(const QString &name)
{
    username = name;
}

// Returns the hostname
QString Platform::getHostname()
{
    static QString hostname = "";
    if (hostname != "") return hostname;

#if defined(Q_OS_ANDROID)
    QJniObject model = QJniObject::getStaticObjectField<jstring>("android/os/Build", "MODEL");
    hostname = model.toString();
    qDebug() << "The hostname is" << hostname;
#elif defined(Q_OS_UNIX)
    hostname = QHostInfo::localHostName().replace(".local", "");
#else
    // Other platforms can be re-added here
    hostname = "Unknown";
#endif

    return hostname;
}

// Returns the platform name
QString Platform::getPlatformName()
{
#if defined(Q_OS_ANDROID)
    return "Android";
#elif defined(Q_OS_UNIX)
    return "Linux";
#else
    // Other platforms can be re-added here
    return "Unknown";
#endif
}

// Returns the platform avatar path
QString Platform::getAvatarPath()
{
#if defined(Q_OS_ANDROID)
    return getAndroidAvatarPath();
#elif defined(Q_OS_UNIX)
    return getLinuxAvatarPath();
#else
    // Other platforms can be re-added here
    return "";
#endif
}

// Returns the platform default output path
QString Platform::getDefaultPath()
{
    QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
#if defined(Q_OS_ANDROID)
    return "/sdcard/Download";
#else
    return downloadsPath;
#endif
}

#if defined(Q_OS_ANDROID)
QString Platform::getAndroidAvatarPath()
{
    QString path="";
    //TODO: how to get android available path
    return path;
}
#endif
#if defined(Q_OS_UNIX)
QString Platform::getLinuxAvatarPath() {
    static QRegularExpression re("^Icon=(.*)$");
    QString path;
    path = QString(getenv("HOME")) + "/.face";
    if (QFile::exists(path)) return path;
    QFile f("/var/lib/AccountsService/users/" + QString(getenv("USER")));
    if (!f.open(QFile::ReadOnly)) return "";
    QTextStream ts(&f);
    QString line;
    bool found = false;
    while (true) {
        line = ts.readLine();
        if (line.isNull()) break;
        if (line.startsWith("Icon=")) {
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                QStringList pathlist = match.capturedTexts();
                path = pathlist[1];
                found = true;
                break;
            }
        }
    }
    f.close();
    if (found && QFile::exists(path)) return path;
    return "";
}
#endif

// Other platform-specific code (Windows, Mac, Symbian, etc.) can be re-added here if needed in the future.
