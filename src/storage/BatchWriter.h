#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include "core/Types.h"

namespace ui_shared {
namespace behavior {

class OperationQueue;

// 批量写入器：在工作线程事件循环中运行
// 主线程通过信号/invokedMethod触发写入
class BatchWriter : public QObject {
    Q_OBJECT
public:
    explicit BatchWriter(OperationQueue* queue, QObject* parent = nullptr);
    ~BatchWriter() override;

    void start();   // moveToThread 到内部 QThread 并启动
    void stop();    // 请求停止并等待线程退出
    void flush();   // 立即写入队列中所有数据（投递到工作线程）

signals:
    void written(int count);
    void error(const QString& message);

private slots:
    void onTimeout();
    void doFlush();

private:
    void writeBatch(int maxCount);

    OperationQueue* m_queue;
    QThread* m_thread = nullptr;
    QTimer* m_timer = nullptr;
    int m_batchSize = 100;
};

} // namespace behavior
} // namespace ui_shared
