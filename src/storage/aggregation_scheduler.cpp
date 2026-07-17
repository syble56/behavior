#include "aggregation_scheduler.h"
#include "aggregator.h"
#include <QDateTime>
#include <QDate>
#include <QTime>

namespace ui_shared {
namespace behavior {

// ============ AggregationWorker ============

AggregationWorker::AggregationWorker(QObject* parent)
    : QObject(parent) {
    aggregator_ = new Aggregator(this);

    hourlyTimer_ = new QTimer(this);
    hourlyTimer_->setInterval(60 * 60 * 1000);
    connect(hourlyTimer_, &QTimer::timeout, this, [this]() {
        aggregateToday();
        aggregateYesterday();
    });

    startupTimer_ = new QTimer(this);
    startupTimer_->setSingleShot(true);
    startupTimer_->setInterval(5000);
    connect(startupTimer_, &QTimer::timeout, this, [this]() {
        aggregateToday();
    });

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(5 * 60 * 1000);
    connect(refreshTimer_, &QTimer::timeout, this, [this]() {
        aggregateToday();
    });
}

void AggregationWorker::startTimers() {
    startupTimer_->start();
    refreshTimer_->start();
    hourlyTimer_->start();
}

void AggregationWorker::stopTimers() {
    hourlyTimer_->stop();
    startupTimer_->stop();
    refreshTimer_->stop();
}

void AggregationWorker::aggregateToday() {
    QDate today = QDate::currentDate();
    aggregator_->aggregateRange(
        QDateTime(today, QTime(0, 0)),
        QDateTime::currentDateTime(),
        Aggregator::Granularity::Day);
}

void AggregationWorker::aggregateYesterday() {
    QDate yesterday = QDate::currentDate().addDays(-1);
    aggregator_->aggregateRange(
        QDateTime(yesterday, QTime(0, 0)),
        QDateTime(yesterday, QTime(23, 59, 59)),
        Aggregator::Granularity::Day);
}

// ============ AggregationScheduler ============

AggregationScheduler::AggregationScheduler(QObject* parent)
    : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new AggregationWorker();
    worker_->moveToThread(thread_);
}

AggregationScheduler::~AggregationScheduler() {
    stop();
    if (worker_) {
        delete worker_;
        worker_ = nullptr;
    }
}

void AggregationScheduler::start() {
    thread_->start();
    // 在工作线程中启动定时器
    QMetaObject::invokeMethod(worker_, &AggregationWorker::startTimers, Qt::QueuedConnection);
}

void AggregationScheduler::stop() {
    if (thread_ && thread_->isRunning()) {
        QMetaObject::invokeMethod(worker_, &AggregationWorker::stopTimers, Qt::QueuedConnection);
        thread_->quit();
        thread_->wait(5000);
    }
}

void AggregationScheduler::triggerAggregation() {
    // 异步投递到工作线程，主线程不阻塞
    QMetaObject::invokeMethod(worker_, &AggregationWorker::aggregateToday, Qt::QueuedConnection);
}

} // namespace behavior
} // namespace ui_shared
