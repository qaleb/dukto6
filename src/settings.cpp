#include "settings.h"
#include "platform.h"

#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include "theme.h"

#include <QDebug>

Settings::Settings(QObject *parent) :
    QObject(parent), mSettings("dukto", "Dukto")
{
    // Ensure the default theme color is set if it's not already in settings
    if (mSettings.value("ThemeColor").toString().isEmpty()) {
        mSettings.setValue("ThemeColor", Theme::DEFAULT_THEME_COLOR);
    }
}

QString Settings::currentPath()
{
#ifdef Q_OS_ANDROID
    // Shared output uses content URIs; this private directory is only the validation root.
    return Platform::getDefaultPath();
#else
    // Retrieve the last saved path (if any)
    QString path = mSettings.value("DestPath", "").toString();
    // qDebug() << "The current path is:" << path;
    // Check if exists
    if (!path.isEmpty() && QFileInfo(path).isDir() && QFileInfo(path).isWritable())
        return path;

    // Else return the default path for this platform
    path = Platform::getDefaultPath();
    if (QDir(path).exists())
        return path;
    else
        return ".";
#endif
}

void Settings::savePath(QString path)
{
    // Save the new path
    mSettings.setValue("DestPath", path);
    mSettings.sync();
}

void Settings::saveWindowGeometry(QByteArray geo)
{
    mSettings.setValue("WindowPosAndSize", geo);
    mSettings.sync();
}

void Settings::saveWindowGeometry(QRect geo)
{
    // QRect staticRect(geo.left(), geo.top(), 360, 600);
    mSettings.setValue("WindowPosAndSize", geo);
    mSettings.sync();
}

QByteArray Settings::windowGeometry()
{
    return mSettings.value("WindowPosAndSize").toByteArray();
}

QRect Settings::windowRect()
{
    return mSettings.value("WindowPosAndSize", QRect(20,40,360,600)).toRect();
}

void Settings::saveThemeColor(QString color)
{
    mSettings.setValue("ThemeColor", color);
    mSettings.sync();
}

QString Settings::themeColor()
{
    return mSettings.value("ThemeColor", Theme::DEFAULT_THEME_COLOR).toString();
}

void Settings::saveThemeMode(const QString &mode)
{
    if (mode != "system" && mode != "light" && mode != "dark") return;
    mSettings.setValue("ThemeMode", mode);
    mSettings.sync();
}

QString Settings::themeMode()
{
    const QString mode = mSettings.value("ThemeMode", "system").toString();
    return mode == "light" || mode == "dark" ? mode : QStringLiteral("system");
}

void Settings::saveShowTermsOnStart(bool show)
{
    mSettings.setValue("R6/ShowTermsOnStart", show);
    mSettings.sync();
}

bool Settings::showTermsOnStart()
{
    return mSettings.value("R6/ShowTermsOnStart", true).toBool();
}

void Settings::saveConfirmBeforeReceive(bool enabled)
{
    mSettings.setValue("R6/ConfirmBeforeReceive", enabled);
    mSettings.sync();
}

bool Settings::confirmBeforeReceive()
{
    return mSettings.value("R6/ConfirmBeforeReceive", false).toBool();
}

QString Settings::buddyName()
{
    // Retrieve the last saved name (if any)
    QString buddyName = mSettings.value("BuddyName", "").toString();

    if (buddyName.isEmpty()) {
        return Platform::getHostname();
    }

    return buddyName;
}

void Settings::saveBuddyName(QString name)
{
    // Save the new name
    mSettings.setValue("BuddyName", name);
    qDebug() << "The buddyName to be saved is:" << name;
    mSettings.sync();

    // Update the static username when the buddyName is set
    Platform::updateUsername(name);
}
