#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>

namespace ui_shared {
namespace behavior {

class Aggregator;

// Worker — 生活在工作线程中，所有定时器和聚合都在这里
class AggregationWorker : public QObject {
    Q_OBJECT
public:
    explicit AggregationWorker(QObject* parent = nullptr);

public slots:
    void startTimers();
    void stopTimers();
    void aggregateToday();

private:
    void aggregateYesterday();

    QTimer* hourlyTimer_ = nullptr;
    QTimer* startupTimer_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    Aggregator* aggregator_ = nullptr;
};

// Scheduler — 生活在主线程，持有 worker + thread
class AggregationScheduler : public QObject {
    Q_OBJECT
public:
    explicit AggregationScheduler(QObject* parent = nullptr);
    ~AggregationScheduler() override;

    void start();
    void stop();
    void triggerAggregation();  // 异步

private:
    QThread* thread_ = nullptr;
    AggregationWorker* worker_ = nullptr;
};

} // namespace behavior
} // namespace ui_shared
