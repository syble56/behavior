#include "aggregation_scheduler.h"
#include "aggregator.h"
#include <QDateTime>
#include <QDate>
#include <QTime>

namespace ui_shared {
namespace behavior {

AggregationScheduler::AggregationScheduler(QObject* parent)
    : QObject(parent), aggregator_(new Aggregator(this)) {
    hourlyTimer_ = new QTimer(this);
    hourlyTimer_->setInterval(60 * 60 * 1000);  // 每小时
    connect(hourlyTimer_, &QTimer::timeout, this, &AggregationScheduler::onHourly);

    startupTimer_ = new QTimer(this);
    startupTimer_->setSingleShot(true);
    startupTimer_->setInterval(5000);  // 启动5秒后
    connect(startupTimer_, &QTimer::timeout, this, &AggregationScheduler::onStartup);

    // 每5分钟刷新今天的 Day 粒度聚合
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(5 * 60 * 1000);
    connect(refreshTimer_, &QTimer::timeout, this, &AggregationScheduler::onRefreshToday);
}

AggregationScheduler::~AggregationScheduler() {
    stop();
}

void AggregationScheduler::start() {
    startupTimer_->start();
    refreshTimer_->start();
    hourlyTimer_->start();
}

void AggregationScheduler::stop() {
    if (hourlyTimer_) hourlyTimer_->stop();
    if (startupTimer_) startupTimer_->stop();
    if (refreshTimer_) refreshTimer_->stop();
}

void AggregationScheduler::triggerAggregation() {
    // 手动触发：聚合今天（Day粒度）
    QDate today = QDate::currentDate();
    aggregator_->aggregateRange(
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
    aggregator_->aggregateRange(
        QDateTime(today, QTime(0, 0)),
        QDateTime::currentDateTime(),
        Aggregator::Granularity::Day);

    // 昨天（Day粒度）— 最终化昨天的数据
    QDate yesterday = today.addDays(-1);
    aggregator_->aggregateRange(
        QDateTime(yesterday, QTime(0, 0)),
        QDateTime(yesterday, QTime(23, 59, 59)),
        Aggregator::Granularity::Day);
}

} // namespace behavior
} // namespace ui_shared
