#include "AggregationScheduler.h"
#include "Aggregator.h"
#include <QDateTime>
#include <QDate>
#include <QTime>

namespace ui_shared {
namespace behavior {

AggregationScheduler::AggregationScheduler(QObject* parent)
    : QObject(parent), m_aggregator(new Aggregator(this)) {
    m_hourlyTimer = new QTimer(this);
    m_hourlyTimer->setInterval(60 * 60 * 1000);  // 每小时
    connect(m_hourlyTimer, &QTimer::timeout, this, &AggregationScheduler::onHourly);

    m_startupTimer = new QTimer(this);
    m_startupTimer->setSingleShot(true);
    m_startupTimer->setInterval(5000);  // 启动5秒后
    connect(m_startupTimer, &QTimer::timeout, this, &AggregationScheduler::onStartup);

    // 每5分钟刷新今天的 Day 粒度聚合
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5 * 60 * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &AggregationScheduler::onRefreshToday);
}

AggregationScheduler::~AggregationScheduler() {
    stop();
}

void AggregationScheduler::start() {
    m_startupTimer->start();
    m_refreshTimer->start();
    m_hourlyTimer->start();
}

void AggregationScheduler::stop() {
    if (m_hourlyTimer) m_hourlyTimer->stop();
    if (m_startupTimer) m_startupTimer->stop();
    if (m_refreshTimer) m_refreshTimer->stop();
}

void AggregationScheduler::triggerAggregation() {
    // 手动触发：聚合今天（Day粒度）
    QDate today = QDate::currentDate();
    m_aggregator->aggregateRange(
        QDateTime(today, QTime(0, 0)),
        QDateTime::currentDateTime(),
        Aggregator::Granularity::Day);
}

void AggregationScheduler::onStartup() {
    // 启动时聚合今天全天（Day粒度）
    triggerAggregation();
}

void AggregationScheduler::onRefreshToday() {
    // 每5分钟刷新今天的 Day 粒度数据
    triggerAggregation();
}

void AggregationScheduler::onHourly() {
    QDate today = QDate::currentDate();

    // 今天（Day粒度）— 刷新，保证最新
    m_aggregator->aggregateRange(
        QDateTime(today, QTime(0, 0)),
        QDateTime::currentDateTime(),
        Aggregator::Granularity::Day);

    // 昨天（Day粒度）— 最终化昨天的数据
    QDate yesterday = today.addDays(-1);
    m_aggregator->aggregateRange(
        QDateTime(yesterday, QTime(0, 0)),
        QDateTime(yesterday, QTime(23, 59, 59)),
        Aggregator::Granularity::Day);
}

} // namespace behavior
} // namespace ui_shared
