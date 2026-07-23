#include "behavior_analytics.h"
#include "config.h"
#include "session.h"
#include "collector/event_filter.h"
#include "collector/event_processor.h"
#include "collector/knob_resolver.h"
#include "storage/operation_queue.h"
#include "storage/database.h"
#include "storage/batch_writer.h"
#include "storage/aggregation_scheduler.h"
#include "storage/cleaner.h"
#include "analyzer/behavior_analyzer.h"
#include "analyzer/analyzer_registry.h"
#include "analyzer/analyzer_base.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <cstdio>

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
    std::unique_ptr<Cleaner> cleaner;
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

    fprintf(stdout, "[I] step0\n"); fflush(stdout);
    qDebug("[Behavior] Initializing...");

    // 1. 打开数据库
    QString dbPath = Config::instance().databasePath();
    qDebug("[Behavior] Database path: %s", qPrintable(dbPath.isEmpty() ? "(default)" : dbPath));
    fprintf(stdout, "[I] opening db: %s\n", qPrintable(dbPath.isEmpty() ? "(default)" : dbPath)); fflush(stdout);
    bool dbOk = Database::instance().open(dbPath);
    qDebug("[Behavior] Database open: %s", dbOk ? "OK" : "FAILED");
    fprintf(stdout, "[I] db open: %s\n", dbOk ? "OK" : "FAILED"); fflush(stdout);

    // 2. 队列
    fprintf(stdout, "[I] queue\n"); fflush(stdout);
    self.impl_->queue = std::make_unique<OperationQueue>();

    // 3. 批量写入器
    fprintf(stdout, "[I] writer\n"); fflush(stdout);
    self.impl_->writer = std::make_unique<BatchWriter>(self.impl_->queue.get());
    self.impl_->writer->start();

    // 恢复上次未正常关闭的会话（必须在新 session 插入之前，否则会把当前 session 也恢复掉）
    int recoveredSessions = Database::instance().recoverUnclosedSessions();
    if (recoveredSessions > 0) {
        fprintf(stdout, "[I] recovered %d unclosed sessions\n", recoveredSessions); fflush(stdout);
    }

    // 4. 会话
    fprintf(stdout, "[I] session\n"); fflush(stdout);
    self.impl_->session = std::make_unique<SessionManager>();
    QString sid = self.impl_->session->start();
    Database::instance().insertSession(self.impl_->session->current());

    // 5. 事件处理器 + 过滤器
    fprintf(stdout, "[I] processor\n"); fflush(stdout);
    self.impl_->processor = std::make_unique<EventProcessor>(self.impl_->queue.get());
    self.impl_->processor->setSessionId(sid);
    
    fprintf(stdout, "[I] connect signal\n"); fflush(stdout);
    // 连接日志信号
    QObject::connect(self.impl_->processor.get(), &EventProcessor::operationRecorded,
                     &self, &BehaviorAnalytics::operationRecorded);
    
    // 旋钮默认映射 (Ctrl+F11→左旋, Ctrl+F12→右旋)
    KnobResolver::instance().loadDefaults();

    fprintf(stdout, "[I] event filter\n"); fflush(stdout);
    self.impl_->filter = std::make_unique<EventFilter>(self.impl_->processor.get());
    fprintf(stdout, "[I] install filter\n"); fflush(stdout);
    self.impl_->filter->install();

    fprintf(stdout, "[I] recover dialogs\n"); fflush(stdout);
    // 补齐上次未正常关闭的弹窗
    self.impl_->processor->recoverUnclosedDialogs();

    // 6. 分析器
    self.impl_->analyzer = std::make_unique<BehaviorAnalyzer>();

    // 7. 聚合调度
    self.impl_->scheduler = std::make_unique<AggregationScheduler>();
    self.impl_->scheduler->start();

    // 8. 异步数据清理器（独立线程，不阻塞主线程）
    fprintf(stdout, "[Main] Creating cleaner...\n"); fflush(stdout);
    self.impl_->cleaner = std::make_unique<Cleaner>();
    fprintf(stdout, "[Main] Starting cleaner...\n"); fflush(stdout);
    self.impl_->cleaner->start();
    fprintf(stdout, "[Main] Triggering async clean...\n"); fflush(stdout);
    // 启动时异步清理一次过期数据（处理上次未正常退出的残留）
    self.impl_->cleaner->cleanAsync(Config::instance().retentionDays());

    self.impl_->initialized = true;
    fprintf(stdout, "[Main] Init complete\n"); fflush(stdout);
    qDebug("[Behavior] Initialized successfully");
}

void BehaviorAnalytics::shutdown() {
    auto& self = instance();
    if (!self.impl_->initialized) return;
    self.impl_->initialized = false;

    // 1. 卸载过滤器
    if (self.impl_->filter) self.impl_->filter->uninstall();

    // 2. 停止聚合调度
    if (self.impl_->scheduler) self.impl_->scheduler->stop();

    // 3. 停止批量写入（flush剩余）— 先刷盘再统计 operationCount
    if (self.impl_->writer) self.impl_->writer->stop();

    // 4. 结束会话（在 writer flush 之后，operationCount 才准确）
    if (self.impl_->session) {
        Session s = self.impl_->session->end();
        QueryFilter f;
        f.sessionId = s.id;
        s.operationCount = static_cast<int>(
            Database::instance().countOperations(f));
        Database::instance().updateSession(s);
    }

    // 5. 异步清理过期数据（在工作线程执行，QEventLoop 等待不阻塞UI）
    if (self.impl_->cleaner) {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(self.impl_->cleaner.get(), &Cleaner::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        self.impl_->cleaner->cleanAsync(Config::instance().retentionDays());
        timeout.start(10000);  // 最多等10秒
        loop.exec();
        // 无论完成还是超时，继续往下走
    }

    // 6. 停止清理器
    if (self.impl_->cleaner) self.impl_->cleaner->stop();

    // 7. 关闭数据库
    Database::instance().close();

    self.impl_->filter.reset();
    self.impl_->processor.reset();
    self.impl_->writer.reset();
    self.impl_->queue.reset();
    self.impl_->scheduler.reset();
    self.impl_->cleaner.reset();
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
