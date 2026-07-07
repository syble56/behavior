#include "behavior_analytics.h"
#include "config.h"
#include "session.h"
#include "collector/event_filter.h"
#include "collector/event_processor.h"
#include "storage/operation_queue.h"
#include "storage/database.h"
#include "storage/batch_writer.h"
#include "storage/aggregation_scheduler.h"
#include "analyzer/behavior_analyzer.h"
#include "analyzer/analyzer_registry.h"
#include "analyzer/analyzer_base.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>

namespace ui_shared {
namespace behavior {

struct BehaviorAnalytics::Impl {
    std::unique_ptr<OperationQueue> queue;
    std::unique_ptr<BatchWriter> writer;
    std::unique_ptr<EventProcessor> processor;
    std::unique_ptr<EventFilter> filter;
    std::unique_ptr<SessionManager> session;
    std::unique_ptr<AggregationScheduler> scheduler;
    std::unique_ptr<BehaviorAnalyzer> analyzer;
    bool initialized = false;
};

BehaviorAnalytics& BehaviorAnalytics::instance() {
    static BehaviorAnalytics inst;
    return inst;
}

BehaviorAnalytics::BehaviorAnalytics()
    : QObject(nullptr), impl_(std::make_unique<Impl>()) {}

BehaviorAnalytics::~BehaviorAnalytics() {
    shutdown();
}

void BehaviorAnalytics::init() {
    auto& self = instance();
    if (self.impl_->initialized) return;

    qDebug("[Behavior] Initializing...");

    // 1. 打开数据库
    QString dbPath = Config::instance().databasePath();
    qDebug("[Behavior] Database path: %s", qPrintable(dbPath.isEmpty() ? "(default)" : dbPath));
    bool dbOk = Database::instance().open(dbPath);
    qDebug("[Behavior] Database open: %s", dbOk ? "OK" : "FAILED");

    // 2. 队列
    self.impl_->queue = std::make_unique<OperationQueue>();

    // 3. 批量写入器
    self.impl_->writer = std::make_unique<BatchWriter>(self.impl_->queue.get());
    self.impl_->writer->start();

    // 4. 会话
    self.impl_->session = std::make_unique<SessionManager>();
    QString sid = self.impl_->session->start();
    Database::instance().insertSession(self.impl_->session->current());

    // 5. 事件处理器 + 过滤器
    self.impl_->processor = std::make_unique<EventProcessor>(self.impl_->queue.get());
    self.impl_->processor->setSessionId(sid);
    
    // 连接日志信号
    QObject::connect(self.impl_->processor.get(), &EventProcessor::operationRecorded,
                     &self, &BehaviorAnalytics::operationRecorded);
    
    self.impl_->filter = std::make_unique<EventFilter>(self.impl_->processor.get());
    self.impl_->filter->install();

    // 补齐上次未正常关闭的弹窗
    self.impl_->processor->recoverUnclosedDialogs();

    // 6. 分析器
    self.impl_->analyzer = std::make_unique<BehaviorAnalyzer>();

    // 7. 聚合调度
    self.impl_->scheduler = std::make_unique<AggregationScheduler>();
    self.impl_->scheduler->start();

    self.impl_->initialized = true;
    qDebug("[Behavior] Initialized successfully");
}

void BehaviorAnalytics::shutdown() {
    auto& self = instance();
    if (!self.impl_->initialized) return;
    self.impl_->initialized = false;

    // 1. 卸载过滤器
    if (self.impl_->filter) self.impl_->filter->uninstall();

    // 2. 结束会话
    if (self.impl_->session) {
        Session s = self.impl_->session->end();
        s.operationCount = static_cast<int>(
            Database::instance().countOperations(QueryFilter{}));
        Database::instance().updateSession(s);
    }

    // 3. 停止聚合调度
    if (self.impl_->scheduler) self.impl_->scheduler->stop();

    // 4. 停止批量写入（flush剩余）
    if (self.impl_->writer) self.impl_->writer->stop();

    // 5. 清理过期数据
    Database::instance().cleanOldData(Config::instance().retentionDays());

    // 6. 关闭数据库
    Database::instance().close();

    self.impl_->filter.reset();
    self.impl_->processor.reset();
    self.impl_->writer.reset();
    self.impl_->queue.reset();
    self.impl_->scheduler.reset();
    self.impl_->analyzer.reset();
    self.impl_->session.reset();
}

void BehaviorAnalytics::setRetentionDays(int days) { Config::instance().setRetentionDays(days); }
void BehaviorAnalytics::setBatchSize(int size) { Config::instance().setBatchSize(size); }
void BehaviorAnalytics::addIgnoreControl(const QString& className) { Config::instance().addIgnoreControl(className); }
void BehaviorAnalytics::setEnabled(bool enabled) { Config::instance().setEnabled(enabled); }
void BehaviorAnalytics::setDatabasePath(const QString& path) { Config::instance().setDatabasePath(path); }

void BehaviorAnalytics::setModule(const QString& module) {
    auto& self = instance();
    if (self.impl_->processor) self.impl_->processor->setModule(module);
}

BehaviorAnalyzer* BehaviorAnalytics::analyzer() {
    return instance().impl_->analyzer.get();
}

void BehaviorAnalytics::registerAnalyzer(const QString& name, AnalyzerBase* analyzer) {
    AnalyzerRegistry::instance().registerAnalyzer(name, analyzer);
}

QList<Operation> BehaviorAnalytics::exportRawData(const QDateTime& start, const QDateTime& end) {
    return instance().impl_->analyzer ? instance().impl_->analyzer->exportRawData(start, end)
                                       : QList<Operation>{};
}

} // namespace behavior
} // namespace ui_shared
