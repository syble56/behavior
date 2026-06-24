#include "OperationQueue.h"

namespace ui_shared {
namespace behavior {

OperationQueue::OperationQueue() = default;
OperationQueue::~OperationQueue() = default;

void OperationQueue::enqueue(Operation&& op) {
    bool drop = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_queue.size() >= HIGH_WATERMARK) {
            // 病态背压：丢弃新事件，保留时间连续性
            ++m_droppedCount;
            drop = true;
        } else {
            m_queue.append(std::move(op));
        }
    }
    if (!drop)
        m_semaphore.release();
}

QList<Operation> OperationQueue::dequeue(int maxCount) {
    QMutexLocker locker(&m_mutex);
    if (maxCount <= 0 || m_queue.isEmpty())
        return {};
    int n = qMin(maxCount, m_queue.size());
    QList<Operation> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i)
        out.append(std::move(m_queue[i]));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_queue.remove(0, n);
#else
    m_queue.erase(m_queue.begin(), m_queue.begin() + n);
#endif
    return out;
}

int OperationQueue::size() const {
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
}

bool OperationQueue::waitForData(int timeoutMs) {
    return m_semaphore.tryAcquire(1, timeoutMs);
}

qint64 OperationQueue::droppedCount() const {
    QMutexLocker locker(&m_mutex);
    return m_droppedCount;
}

} // namespace behavior
} // namespace ui_shared
