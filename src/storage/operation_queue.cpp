#include "operation_queue.h"

namespace ui_shared {
namespace behavior {

OperationQueue::OperationQueue() = default;
OperationQueue::~OperationQueue() = default;

void OperationQueue::enqueue(Operation&& op) {
    bool drop = false;
    {
        QMutexLocker locker(&mutex_);
        if (queue_.size() >= HIGH_WATERMARK) {
            // 病态背压：丢弃新事件，保留时间连续性
            ++droppedCount_;
            drop = true;
        } else {
            queue_.append(std::move(op));
        }
    }
    if (!drop)
        semaphore_.release();
}

QList<Operation> OperationQueue::dequeue(int maxCount) {
    QMutexLocker locker(&mutex_);
    if (maxCount <= 0 || queue_.isEmpty())
        return {};
    int n = qMin(maxCount, queue_.size());
    QList<Operation> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i)
        out.append(std::move(queue_[i]));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    queue_.remove(0, n);
#else
    queue_.erase(queue_.begin(), queue_.begin() + n);
#endif
    return out;
}

int OperationQueue::size() const {
    QMutexLocker locker(&mutex_);
    return queue_.size();
}

bool OperationQueue::waitForData(int timeoutMs) {
    return semaphore_.tryAcquire(1, timeoutMs);
}

qint64 OperationQueue::droppedCount() const {
    QMutexLocker locker(&mutex_);
    return droppedCount_;
}

} // namespace behavior
} // namespace ui_shared
