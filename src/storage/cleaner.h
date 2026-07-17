#pragma once

#include <QObject>
#include <QThread>

namespace ui_shared {
namespace behavior {

// 异步数据清理器：在工作线程中执行过期数据清理，不阻塞主线程
class Cleaner : public QObject {
    Q_OBJECT
public:
    explicit Cleaner(QObject* parent = nullptr);
    ~Cleaner() override;

    void start();   // moveToThread 到内部 QThread 并启动
    void stop();    // 请求停止并等待线程退出

    // 异步触发清理（投递到工作线程，不阻塞调用方）
    void cleanAsync(int retentionDays);

signals:
    void finished(int removed);
    void error(const QString& message);

private:
    void doClean(int retentionDays);
    QThread* thread_ = nullptr;
};

} // namespace behavior
} // namespace ui_shared
