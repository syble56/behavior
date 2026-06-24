#pragma once

#include <QObject>
#include <memory>
#include "Types.h"

namespace ui_shared {
namespace behavior {

class BehaviorAnalyzer;
class EventFilter;
class OperationQueue;
class BatchWriter;
class AggregationScheduler;
class SessionManager;
class AnalyzerBase;

class BehaviorAnalytics : public QObject {
    Q_OBJECT
public:
    static BehaviorAnalytics& instance();

    // 初始化（一行启动）
    static void init();
    // 关闭
    static void shutdown();

    // 配置
    static void setRetentionDays(int days);
    static void setBatchSize(int size);
    static void addIgnoreControl(const QString& className);
    static void setEnabled(bool enabled);
    static void setDatabasePath(const QString& path);

    // 分析器统一入口
    static BehaviorAnalyzer* analyzer();

    // 扩展
    static void registerAnalyzer(const QString& name, AnalyzerBase* analyzer);
    static QList<Operation> exportRawData(const QDateTime& start, const QDateTime& end);

signals:
    void analysisCompleted(const QString& name, const AnalysisResult& result);
    void operationRecorded(const QString& logMessage);

private:
    BehaviorAnalytics();
    ~BehaviorAnalytics() override;
    Q_DISABLE_COPY(BehaviorAnalytics)

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace behavior
} // namespace ui_shared
