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
    m_hourlyTimer->setInterval(60 * 60 * 1000);
    connect(m_hourlyTimer, &QTimer::timeout, this, &AggregationScheduler::onHourly);

    m_startupTimer = new QTimer(this);
    m_startupTimer->setSingleShot(true);
    m_startupTimer->setInterval(5000);
    connect(m_startupTimer, &QTimer::timeout, this, &AggregationScheduler::onStartup);
}

AggregationScheduler::~AggregationScheduler() {
    stop();
}

void AggregationScheduler::start() {
    m_startupTimer->start();
    m_hourlyTimer->start();
}

void AggregationScheduler::stop() {
    if (m_hourlyTimer) m_hourlyTimer->stop();
    if (m_startupTimer) m_startupTimer->stop();
}

void AggregationScheduler::triggerAggregation() {
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addSecs(-3600);
    m_aggregator->aggregateRange(start, end, Aggregator::Granularity::Hour);
}

void AggregationScheduler::onStartup() {
    triggerAggregation();
}

void AggregationScheduler::onHourly() {
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addSecs(-3600);
    m_aggregator->aggregateRange(start, end, Aggregator::Granularity::Hour);

    QDate yesterday = QDate::currentDate().addDays(-1);
    m_aggregator->aggregateRange(
        QDateTime(yesterday, QTime(0, 0)),
        QDateTime(yesterday, QTime(23, 59, 59)),
        Aggregator::Granularity::Day);
}

} // namespace behavior
} // namespace ui_shared
