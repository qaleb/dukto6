#pragma once
#include <QFile>
#include <QSaveFile>
#include <memory>

// Desktop: QSaveFile. Android: a pending shared-storage item written through its FD.
class ReceivedFile
{
public:
    ~ReceivedFile();
    static std::unique_ptr<ReceivedFile> local(const QString &path, QString *error);
    static std::unique_ptr<ReceivedFile> shared(const QString &relative, qint64 size,
                                               const QString &tree, QString *error);
    qint64 write(const char *data, qint64 size);
    bool commit();
    QString location() const { return mLocation; }
    QString displayName() const { return mDisplayName; }
    QString errorString() const { return mError; }
    static QString selectedTree();
    static bool sharedDirectory(const QString &relative, const QString &tree, QString *error);
    static QString sharedRootDirectory(const QString &name, const QString &tree, QString *error);
    static void recoverPending();
private:
    ReceivedFile() = default;
    std::unique_ptr<QSaveFile> mLocal;
    std::unique_ptr<QFile> mShared;
    QString mLocation, mDisplayName, mFinalName, mError;
    bool mPending = false;
};
