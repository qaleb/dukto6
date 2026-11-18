#include "receivedfile.h"
#include <QFileInfo>
#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace {
QJniObject context() { return QNativeInterface::QAndroidApplication::context(); }
QJsonObject result(const QJniObject &value) {
    return QJsonDocument::fromJson(value.toString().toUtf8()).object();
}
void discard(const QString &uri) {
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoStorage", "discard",
        "(Landroid/content/Context;Ljava/lang/String;)V", context().object<jobject>(),
        QJniObject::fromString(uri).object<jstring>());
}
}
#endif

ReceivedFile::~ReceivedFile()
{
#ifdef Q_OS_ANDROID
    mShared.reset(); // Close the descriptor before removing the pending item.
    if (mPending) discard(mLocation);
#endif
}

std::unique_ptr<ReceivedFile> ReceivedFile::local(const QString &path, QString *error)
{
    auto file = std::unique_ptr<ReceivedFile>(new ReceivedFile);
    file->mLocal = std::make_unique<QSaveFile>(path);
    file->mLocal->setDirectWriteFallback(false);
    if (!file->mLocal->open(QIODevice::WriteOnly)) {
        *error = file->mLocal->errorString(); return {};
    }
    file->mLocation = path;
    file->mDisplayName = QFileInfo(path).fileName();
    return file;
}

std::unique_ptr<ReceivedFile> ReceivedFile::shared(const QString &relative, qint64 size,
                                                 const QString &tree, QString *error)
{
#ifdef Q_OS_ANDROID
    const auto object = result(QJniObject::callStaticObjectMethod("idv/coolshou/DuktoStorage", "beginFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
        context().object<jobject>(), QJniObject::fromString(relative).object<jstring>(),
        QJniObject::fromString(tree).object<jstring>()));
    const int fd = object.value("fd").toInt(-1);
    if (fd < 0) { *error = object.value("error").toString(); return {}; }
    auto file = std::unique_ptr<ReceivedFile>(new ReceivedFile);
    file->mLocation = object.value("uri").toString();
    file->mDisplayName = object.value("name").toString();
    file->mFinalName = object.value("finalName").toString();
    file->mPending = true;
    file->mShared = std::make_unique<QFile>();
    if (!file->mShared->open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd); *error = file->mShared->errorString(); return {};
    }
    struct statvfs storage {};
    if (::fstatvfs(fd, &storage) == 0 && storage.f_frsize > 0
        && (quint64(size) / storage.f_frsize + (quint64(size) % storage.f_frsize != 0)) > storage.f_bavail) {
        *error = QObject::tr("Not enough space on the receiving storage."); return {};
    }
    return file;
#else
    Q_UNUSED(relative); Q_UNUSED(size); Q_UNUSED(tree);
    *error = QObject::tr("Shared Android storage is unavailable."); return {};
#endif
}

qint64 ReceivedFile::write(const char *data, qint64 size)
{
    auto *device = mLocal ? static_cast<QFileDevice *>(mLocal.get()) : mShared.get();
    const auto count = device->write(data, size);
    if (count < 0) mError = device->errorString();
    return count;
}

bool ReceivedFile::commit()
{
    if (mLocal) {
        const bool ok = mLocal->commit();
        if (!ok) mError = mLocal->errorString();
        return ok;
    }
#ifdef Q_OS_ANDROID
    if (!mShared->flush()) { mError = mShared->errorString(); return false; }
    struct stat info {};
    if (::fstat(mShared->handle(), &info) == 0 && S_ISREG(info.st_mode) && ::fsync(mShared->handle()) != 0) {
        mError = QString::fromLocal8Bit(std::strerror(errno)); return false;
    }
    mShared.reset();
    const auto object = result(QJniObject::callStaticObjectMethod("idv/coolshou/DuktoStorage", "publish",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
        context().object<jobject>(), QJniObject::fromString(mLocation).object<jstring>(),
        QJniObject::fromString(mFinalName).object<jstring>()));
    if (!object.contains("uri")) { mError = object.value("error").toString(); return false; }
    mLocation = object.value("uri").toString();
    mDisplayName = object.value("name").toString();
    mPending = false;
    return true;
#else
    return false;
#endif
}

QString ReceivedFile::selectedTree()
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticObjectMethod("idv/coolshou/DuktoStorage", "selectedTree",
        "(Landroid/content/Context;)Ljava/lang/String;", context().object<jobject>()).toString();
#else
    return {};
#endif
}

bool ReceivedFile::sharedDirectory(const QString &relative, const QString &tree, QString *error)
{
#ifdef Q_OS_ANDROID
    const auto object = result(QJniObject::callStaticObjectMethod("idv/coolshou/DuktoStorage", "directory",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
        context().object<jobject>(), QJniObject::fromString(relative).object<jstring>(),
        QJniObject::fromString(tree).object<jstring>()));
    if (object.value("ok").toBool()) return true;
    *error = object.value("error").toString(); return false;
#else
    Q_UNUSED(relative); Q_UNUSED(tree); Q_UNUSED(error); return false;
#endif
}

QString ReceivedFile::sharedRootDirectory(const QString &name, const QString &tree, QString *error)
{
#ifdef Q_OS_ANDROID
    const auto object = result(QJniObject::callStaticObjectMethod("idv/coolshou/DuktoStorage", "reserveRootDirectory",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
        context().object<jobject>(), QJniObject::fromString(name).object<jstring>(),
        QJniObject::fromString(tree).object<jstring>()));
    if (object.contains("name")) return object.value("name").toString();
    *error = object.value("error").toString(); return {};
#else
    Q_UNUSED(name); Q_UNUSED(tree); Q_UNUSED(error); return {};
#endif
}

void ReceivedFile::recoverPending()
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoStorage", "recoverPending",
        "(Landroid/content/Context;)V", context().object<jobject>());
#endif
}
