#include "batch_writer.h"
#include "operation_queue.h"
#include "database.h"
#include "core/config.h"
#include <QMetaObject>
#include <QTimer>
#include <limits>

namespace ui_shared {
namespace behavior {

BatchWriter::BatchWriter(OperationQueue* queue, QObject* parent)
    : QObject(parent), queue_(queue) {}

BatchWriter::~BatchWriter() {
    stop();
}

void BatchWriter::start() {
    if (thread_) return;
    batchSize_ = Config::instance().batchSize();

    thread_ = new QThread;
    this->moveToThread(thread_);

    // 定时器需要在移动到新线程后创建，作为本对象的子对象
    // 使用 QueuedConnection 在新线程中创建定时器
    QMetaObject::invokeMethod(this, [this]() {
        timer_ = new QTimer(this);
        timer_->setInterval(Config::instance().batchTimeoutMs());
        connect(timer_, &QTimer::timeout, this, &BatchWriter::onTimeout);
        timer_->start();
    }, Qt::QueuedConnection);

    thread_->start();
}

void BatchWriter::stop() {
    if (!thread_) return;
    // 先flush剩余
    QMetaObject::invokeMethod(this, &BatchWriter::doFlush, Qt::BlockingQueuedConnection);
    thread_->quit();
    thread_->wait();
    delete thread_;
    thread_ = nullptr;
    timer_ = nullptr;  // 已随this销毁
}

void BatchWriter::flush() {
    if (!thread_) return;
    QMetaObject::invokeMethod(this, &BatchWriter::doFlush, Qt::QueuedConnection);
}

void BatchWriter::doFlush() {
    writeBatch(std::numeric_limits<int>::max());
}

void BatchWriter::onTimeout() {
    writeBatch(batchSize_);
}

void BatchWriter::writeBatch(int maxCount) {
    if (!queue_) return;
    // 等待少量数据可用
    if (maxCount < std::numeric_limits<int>::max() && queue_->size() == 0) {
        return;
    }
    QList<Operation> ops = queue_->dequeue(maxCount);
    if (ops.isEmpty()) return;
    if (Database::instance().batchInsert(ops)) {
        emit written(ops.size());
    } else {
        emit error(QStringLiteral("batch insert failed"));
    }
}

} // namespace behavior
} // namespace ui_shared
