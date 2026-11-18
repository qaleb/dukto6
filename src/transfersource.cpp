#include "transfersource.h"
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QSet>
#include <QStorageInfo>
#include <QStandardPaths>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#include <QCoreApplication>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <QElapsedTimer>
#endif

namespace {
bool appendLocal(QList<TransferSource> &out, const QString &path, const QString &name, QString *error, const std::atomic_bool &cancelled) {
    if (cancelled) { *error = QObject::tr("Cancelled"); return false; }
    if (out.size() >= 1000000 || name.toUtf8().size() > 4096 || name.count('/') > 128) { *error = QObject::tr("Too many files or folder nesting is too deep."); return false; }
    QFileInfo info(path);
    if (info.isSymLink() || !info.exists() || !info.isReadable() || (!info.isDir() && !info.isFile())) {
        *error = QObject::tr("Cannot read %1. Select the file again. Symbolic links are not sent.").arg(path); return false;
    }
    out.append({info.absoluteFilePath(), name, info.isDir() ? -1 : info.size(), info.isDir(), {}, {}});
    if (info.isDir()) {
        const auto entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto &entry : entries)
            if (!appendLocal(out, entry.absoluteFilePath(), name + '/' + entry.fileName(), error, cancelled)) return false;
    }
    return true;
}
}

QList<TransferSource> prepareSources(const QStringList &urls, QString *error, const std::atomic_bool &cancelled) {
    QList<TransferSource> result;
    QSet<QString> rootNames;
    for (const QString &value : urls) {
        QUrl url(value);
        QString path = url.isLocalFile() ? url.toLocalFile() : value;
#ifdef Q_OS_ANDROID
        if (url.scheme() == "content") {
            QJniObject context = QNativeInterface::QAndroidApplication::context();
            QJniObject metadata = QJniObject::callStaticObjectMethod("idv/coolshou/DuktoBridge", "openSource",
                "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", context.object<jobject>(), QJniObject::fromString(value).object<jstring>());
            const QStringList fields = metadata.toString().split('\n');
            if (fields.size() != 4) { *error = QObject::tr("Cannot access the shared file. Share or select it again."); return {}; }
            int fd = fields[0].toInt();
            TransferSource source{value, fields[2], fields[1].toLongLong(), false, std::make_shared<QFile>(), {}};
            if (fd < 0 || !source.stream->open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
                if (fd >= 0) ::close(fd);
                *error = QObject::tr("The shared file is no longer readable. Select it again."); return {};
            }
            const qint64 offset = fields[3].toLongLong();
            if (offset > 0 && !source.stream->seek(offset)) { *error = QObject::tr("Cannot read this provider's file. Save a local copy and select it again."); return {}; }
            ::fcntl(fd,F_SETFL,::fcntl(fd,F_GETFL) | O_NONBLOCK);
            if (source.size < 0) {
                QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
                source.staging = std::make_shared<QTemporaryDir>(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/share-XXXXXX");
                if (!source.staging->isValid()) { *error = QObject::tr("Cannot create temporary storage."); return {}; }
                QString staged = source.staging->filePath(source.name);
                QFile output(staged);
                if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { *error = output.errorString(); return {}; }
                QByteArray buffer(64 * 1024, Qt::Uninitialized);
                while (!cancelled) {
                    qint64 count = readSource(source, buffer.data(), buffer.size(), cancelled);
                    if (count < 0) { *error = source.stream->errorString(); return {}; }
                    if (!count) break;
                    if (QStorageInfo(source.staging->path()).bytesAvailable() < count || output.write(buffer.constData(),count) != count) {
                        *error = QObject::tr("Not enough temporary storage, or a disk write failed."); return {};
                    }
                }
                if (cancelled) { *error = QObject::tr("Cancelled"); return {}; }
                if (!output.flush()) { *error = output.errorString(); return {}; }
                source.size = output.size(); output.close(); source.stream.reset(); source.location = staged;
            } else source.stream.reset(); // Do not hold one provider descriptor per selected file.
            QString original = source.name;
            for (int n = 2; rootNames.contains(source.name); ++n) source.name = QString::number(n) + '-' + original;
            rootNames.insert(source.name); result.append(source); continue;
        }
#endif
        if (!url.scheme().isEmpty() && !url.isLocalFile()
#ifdef Q_OS_WIN
            && !(value.size() > 1 && value[1] == ':')
#endif
        ) { *error = QObject::tr("Unsupported file location: %1").arg(value); return {}; }
        QString name = QFileInfo(path).fileName();
        if (name.isEmpty()) { *error = QObject::tr("Select a file or folder rather than a filesystem root."); return {}; }
        if (name == "___DUKTO___TEXT___") name = "Shared-" + name;
        QString original = name;
        for (int n = 2; rootNames.contains(name); ++n) name = QString::number(n) + '-' + original;
        rootNames.insert(name);
        if (!appendLocal(result, path, name, error, cancelled)) return {};
    }
    if (result.isEmpty()) *error = QObject::tr("No readable files selected.");
    return result;
}
bool openSource(TransferSource &source, QString *error) {
    if (source.directory) return true;
    if (source.stream && source.stream->isOpen()) return true;
#ifdef Q_OS_ANDROID
    if (source.location.startsWith("content:")) {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        const auto metadata = QJniObject::callStaticObjectMethod("idv/coolshou/DuktoBridge", "openSource",
            "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", context.object<jobject>(), QJniObject::fromString(source.location).object<jstring>());
        const auto fields = metadata.toString().split('\n');
        if (fields.size() != 4) { *error = QObject::tr("The shared file is no longer readable. Share it again."); return false; }
        const int fd = fields[0].toInt();
        if (fd < 0 || fields[1].toLongLong() != source.size) {
            if (fd >= 0) ::close(fd);
            *error = QObject::tr("The shared file changed. Share it again."); return false;
        }
        source.stream = std::make_shared<QFile>();
        if (!source.stream->open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            ::close(fd); *error = QObject::tr("The shared file could not be opened."); return false;
        }
        const auto offset = fields[3].toLongLong();
        if (offset > 0 && !source.stream->seek(offset)) { source.stream.reset(); *error = QObject::tr("This provider's file cannot be read. Save a local copy first."); return false; }
        ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
        return true;
    }
#endif
    source.stream = std::make_shared<QFile>(source.location);
    if (!source.stream->open(QIODevice::ReadOnly) || source.stream->size() != source.size) {
        *error = QObject::tr("File changed or is no longer readable: %1. Select it again.").arg(source.name); return false;
    }
    return true;
}

qint64 readSource(TransferSource &source, char *buffer, qint64 maxBytes, const std::atomic_bool &cancelled) {
#ifdef Q_OS_ANDROID
    if (source.location.startsWith("content:")) {
        QElapsedTimer timeout; timeout.start();
        const int fd = source.stream->handle();
        while (!cancelled && timeout.elapsed() < 30000) {
            pollfd descriptor{fd,POLLIN,0};
            int result = ::poll(&descriptor,1,100);
            if (result < 0 && errno != EINTR) return -1;
            if (result <= 0) continue;
            const auto count = ::read(fd,buffer,size_t(maxBytes));
            if (count >= 0) return count;
            if (errno != EINTR && errno != EAGAIN) return -1;
        }
        return -1;
    }
#endif
    if (cancelled) return -1;
    return source.stream->read(buffer,maxBytes);
}
