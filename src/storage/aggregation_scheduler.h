#pragma once

#include <QObject>
#include <QTimer>

namespace ui_shared {
namespace behavior {

class Aggregator;

class AggregationScheduler : public QObject {
    Q_OBJECT
public:
    explicit AggregationScheduler(QObject* parent = nullptr);
    ~AggregationScheduler() override;

    void start();
    void stop();
    void triggerAggregation();

private slots:
    void onStartup();
    void onRefreshToday();
    void onHourly();

private:
    QTimer* hourlyTimer_ = nullptr;
    QTimer* startupTimer_ = nullptr;
    QTimer* refreshTimer_ = nullptr;   // 定时刷新今天
    Aggregator* aggregator_ = nullptr;
};

} // namespace behavior
} // namespace ui_shared
