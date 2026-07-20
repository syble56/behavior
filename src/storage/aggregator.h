#pragma once

#include <QObject>
#include <QDateTime>
#include <QDate>

namespace ui_shared {
namespace behavior {

class Aggregator : public QObject {
    Q_OBJECT
public:
    enum class Granularity {
        Hour,
        Day
    };

    explicit Aggregator(QObject* parent = nullptr);

    // 聚合指定时间范围
    void aggregateRange(const QDateTime& start, const QDateTime& end,
                        Granularity g = Granularity::Hour);

signals:
    void aggregationCompleted(int recordsProcessed);
    void aggregationFailed(const QString& error);

private:
    void aggregateOperations(const QDateTime& start, const QDateTime& end, Granularity g);
    void aggregateModules(const QDateTime& start, const QDateTime& end, Granularity g);
    void aggregateInputs(const QDateTime& start, const QDateTime& end, Granularity g);
    void aggregateHeatmap(const QDateTime& start, const QDateTime& end, Granularity g);
    void aggregateDialogs(const QDateTime& start, const QDateTime& end, Granularity g);
    void aggregateTimeDistribution(const QDateTime& start, const QDateTime& end, Granularity g);

    QString formatTimeBucket(const QDateTime& time, Granularity g);
};

} // namespace behavior
} // namespace ui_shared
