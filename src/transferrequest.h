#pragma once

#include <QMetaType>
#include <QStringList>
#include <QUuid>
#include <QTemporaryDir>
#include <memory>
#include <atomic>

// Value types cross the UI/I/O boundary. A recipient is captured at submission.
struct TransferRequest {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString host;
    quint16 port = 4644;
    QString recipient;
    QStringList sources;
    QString text;
    bool isText = false;
    int attempts = 0;
    std::shared_ptr<QTemporaryDir> temporary;
    std::shared_ptr<std::atomic_bool> cancellation = std::make_shared<std::atomic_bool>(false);
};
Q_DECLARE_METATYPE(TransferRequest)
