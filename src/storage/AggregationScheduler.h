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
    void onHourly();

private:
    QTimer* m_hourlyTimer = nullptr;
    QTimer* m_startupTimer = nullptr;
    Aggregator* m_aggregator = nullptr;
};

} // namespace behavior
} // namespace ui_shared
