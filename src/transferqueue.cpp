#include "transferqueue.h"
#include <QTimer>

void TransferQueue::enqueue(TransferRequest request)
{
    mPending.enqueue(std::move(request));
    processNext();
}

void TransferQueue::processNext()
{
    if (mActive || mReceiving || mPaused || mPending.isEmpty()) return;
    mActive = mPending.dequeue();
    mActive->cancellation = std::make_shared<std::atomic_bool>(false);
    ++mActive->attempts;
    emit startRequested(*mActive);
}

void TransferQueue::finish(bool retry)
{
    if (!mActive) return;
    if (retry && canRetry()) mPending.prepend(*mActive);
    mActive.reset();
    QTimer::singleShot(0, this, &TransferQueue::processNext);
}

void TransferQueue::setReceiving(bool receiving)
{
    mReceiving = receiving;
    if (!receiving) QTimer::singleShot(0, this, &TransferQueue::processNext);
}

void TransferQueue::setPaused(bool paused)
{
    mPaused = paused;
    if (!paused) QTimer::singleShot(0, this, &TransferQueue::processNext);
}

void TransferQueue::clear()
{
    mPending.clear();
    mActive.reset();
}
