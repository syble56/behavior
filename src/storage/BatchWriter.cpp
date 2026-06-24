#include "BatchWriter.h"
#include "OperationQueue.h"
#include "Database.h"
#include "core/Config.h"
#include <QMetaObject>
#include <QTimer>
#include <limits>

namespace ui_shared {
namespace behavior {

BatchWriter::BatchWriter(OperationQueue* queue, QObject* parent)
    : QObject(parent), m_queue(queue) {}

BatchWriter::~BatchWriter() {
    stop();
}

void BatchWriter::start() {
    if (m_thread) return;
    m_batchSize = Config::instance().batchSize();

    m_thread = new QThread;
    this->moveToThread(m_thread);

    // 定时器需要在移动到新线程后创建，作为本对象的子对象
    // 使用 QueuedConnection 在新线程中创建定时器
    QMetaObject::invokeMethod(this, [this]() {
        m_timer = new QTimer(this);
        m_timer->setInterval(Config::instance().batchTimeoutMs());
        connect(m_timer, &QTimer::timeout, this, &BatchWriter::onTimeout);
        m_timer->start();
    }, Qt::QueuedConnection);

    m_thread->start();
}

void BatchWriter::stop() {
    if (!m_thread) return;
    // 先flush剩余
    QMetaObject::invokeMethod(this, &BatchWriter::doFlush, Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    m_timer = nullptr;  // 已随this销毁
}

void BatchWriter::flush() {
    if (!m_thread) return;
    QMetaObject::invokeMethod(this, &BatchWriter::doFlush, Qt::QueuedConnection);
}

void BatchWriter::doFlush() {
    writeBatch(std::numeric_limits<int>::max());
}

void BatchWriter::onTimeout() {
    writeBatch(m_batchSize);
}

void BatchWriter::writeBatch(int maxCount) {
    if (!m_queue) return;
    // 等待少量数据可用
    if (maxCount < std::numeric_limits<int>::max() && m_queue->size() == 0)
        return;
    QList<Operation> ops = m_queue->dequeue(maxCount);
    if (ops.isEmpty()) return;
    if (Database::instance().batchInsert(ops)) {
        emit written(ops.size());
    } else {
        emit error(QStringLiteral("batch insert failed"));
    }
}

} // namespace behavior
} // namespace ui_shared
