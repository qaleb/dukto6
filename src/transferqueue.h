#pragma once

#include <QObject>
#include <QQueue>
#include <optional>
#include "transferrequest.h"

// Owns recipient/content snapshots and temporary resources, independently of QML.
class TransferQueue : public QObject
{
    Q_OBJECT
public:
    explicit TransferQueue(QObject *parent = nullptr) : QObject(parent) {}
    void enqueue(TransferRequest request);
    void finish(bool retry = false);
    void setReceiving(bool receiving);
    void setPaused(bool paused);
    void clear();
    int queuedCount() const { return mPending.size(); }
    bool hasActive() const { return mActive.has_value(); }
    bool hasWork() const { return hasActive() || !mPending.isEmpty(); }
    bool canRetry() const { return mActive && mActive->attempts < 3; }
signals:
    void startRequested(TransferRequest request);
private:
    void processNext();
    QQueue<TransferRequest> mPending;
    std::optional<TransferRequest> mActive;
    bool mReceiving = false;
    bool mPaused = false;
};
