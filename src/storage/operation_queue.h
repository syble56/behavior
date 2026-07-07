#pragma once

#include <QList>
#include <QMutex>
#include <QSemaphore>
#include "core/types.h"

namespace ui_shared {
namespace behavior {

// 操作队列：主线程入队，工作线程出队
// 互斥保护（QMutex+QSemaphore），单生产者单消费者
class OperationQueue {
public:
    OperationQueue();
    ~OperationQueue();

    // 入队（主线程）
    void enqueue(Operation&& op);

    // 批量出队（工作线程），最多 maxCount 条
    QList<Operation> dequeue(int maxCount);

    // 当前队列大小
    int size() const;

    // 等待数据可用，返回 true 表示有数据或超时
    bool waitForData(int timeoutMs);

    // 丢弃计数（病态背压时累计）
    qint64 droppedCount() const;

private:
    mutable QMutex mutex_;
    QList<Operation> queue_;
    QSemaphore semaphore_;
    qint64 droppedCount_ = 0;

    static constexpr int HIGH_WATERMARK = 50000;
};

} // namespace behavior
} // namespace ui_shared
