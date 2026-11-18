#pragma once
#include <QFile>
#include <QTemporaryDir>
#include <memory>
#include <atomic>

struct TransferSource {
    QString location;
    QString name;
    qint64 size = 0;
    bool directory = false;
    std::shared_ptr<QFile> stream;
    std::shared_ptr<QTemporaryDir> staging;
};

// Called only on the I/O worker; content providers and directory scans never run on the UI thread.
QList<TransferSource> prepareSources(const QStringList &urls, QString *error, const std::atomic_bool &cancelled);
bool openSource(TransferSource &source, QString *error);

qint64 readSource(TransferSource &source, char *buffer, qint64 maxBytes, const std::atomic_bool &cancelled);
