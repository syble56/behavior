#include "BehaviorAnalytics.h"
#include "Config.h"
#include "Session.h"
#include "collector/EventFilter.h"
#include "collector/EventProcessor.h"
#include "storage/OperationQueue.h"
#include "storage/Database.h"
#include "storage/BatchWriter.h"
#include "storage/AggregationScheduler.h"
#include "analyzer/BehaviorAnalyzer.h"
#include "analyzer/AnalyzerRegistry.h"
#include "analyzer/AnalyzerBase.h"
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
    : QObject(nullptr), m_impl(std::make_unique<Impl>()) {}

BehaviorAnalytics::~BehaviorAnalytics() {
    shutdown();
}

void BehaviorAnalytics::init() {
    auto& self = instance();
    if (self.m_impl->initialized) return;

    qDebug("[Behavior] Initializing...");

    // 1. 打开数据库
    QString dbPath = Config::instance().databasePath();
    qDebug("[Behavior] Database path: %s", qPrintable(dbPath.isEmpty() ? "(default)" : dbPath));
    bool dbOk = Database::instance().open(dbPath);
    qDebug("[Behavior] Database open: %s", dbOk ? "OK" : "FAILED");

    // 2. 队列
    self.m_impl->queue = std::make_unique<OperationQueue>();

    // 3. 批量写入器
    self.m_impl->writer = std::make_unique<BatchWriter>(self.m_impl->queue.get());
    self.m_impl->writer->start();

    // 4. 会话
    self.m_impl->session = std::make_unique<SessionManager>();
    QString sid = self.m_impl->session->start();
    Database::instance().insertSession(self.m_impl->session->current());

    // 5. 事件处理器 + 过滤器
    self.m_impl->processor = std::make_unique<EventProcessor>(self.m_impl->queue.get());
    self.m_impl->processor->setSessionId(sid);
    
    // 连接日志信号
    QObject::connect(self.m_impl->processor.get(), &EventProcessor::operationRecorded,
                     &self, &BehaviorAnalytics::operationRecorded);
    
    self.m_impl->filter = std::make_unique<EventFilter>(self.m_impl->processor.get());
    self.m_impl->filter->install();

    // 6. 分析器
    self.m_impl->analyzer = std::make_unique<BehaviorAnalyzer>();

    // 7. 聚合调度
    self.m_impl->scheduler = std::make_unique<AggregationScheduler>();
    self.m_impl->scheduler->start();

    self.m_impl->initialized = true;
    qDebug("[Behavior] Initialized successfully");
}

void BehaviorAnalytics::shutdown() {
    auto& self = instance();
    if (!self.m_impl->initialized) return;
    self.m_impl->initialized = false;

    // 1. 卸载过滤器
    if (self.m_impl->filter) self.m_impl->filter->uninstall();

    // 2. 结束会话
    if (self.m_impl->session) {
        Session s = self.m_impl->session->end();
        s.operationCount = static_cast<int>(
            Database::instance().countOperations(QueryFilter{}));
        Database::instance().updateSession(s);
    }

    // 3. 停止聚合调度
    if (self.m_impl->scheduler) self.m_impl->scheduler->stop();

    // 4. 停止批量写入（flush剩余）
    if (self.m_impl->writer) self.m_impl->writer->stop();

    // 5. 清理过期数据
    Database::instance().cleanOldData(Config::instance().retentionDays());

    // 6. 关闭数据库
    Database::instance().close();

    self.m_impl->filter.reset();
    self.m_impl->processor.reset();
    self.m_impl->writer.reset();
    self.m_impl->queue.reset();
    self.m_impl->scheduler.reset();
    self.m_impl->analyzer.reset();
    self.m_impl->session.reset();
}

void BehaviorAnalytics::setRetentionDays(int days) { Config::instance().setRetentionDays(days); }
void BehaviorAnalytics::setBatchSize(int size) { Config::instance().setBatchSize(size); }
void BehaviorAnalytics::addIgnoreControl(const QString& className) { Config::instance().addIgnoreControl(className); }
void BehaviorAnalytics::setEnabled(bool enabled) { Config::instance().setEnabled(enabled); }
void BehaviorAnalytics::setDatabasePath(const QString& path) { Config::instance().setDatabasePath(path); }

BehaviorAnalyzer* BehaviorAnalytics::analyzer() {
    return instance().m_impl->analyzer.get();
}

void BehaviorAnalytics::registerAnalyzer(const QString& name, AnalyzerBase* analyzer) {
    AnalyzerRegistry::instance().registerAnalyzer(name, analyzer);
}

QList<Operation> BehaviorAnalytics::exportRawData(const QDateTime& start, const QDateTime& end) {
    return instance().m_impl->analyzer ? instance().m_impl->analyzer->exportRawData(start, end)
                                       : QList<Operation>{};
}

} // namespace behavior
} // namespace ui_shared
