#pragma once

#include <QObject>
#include <QDateTime>
#include <memory>
#include "core/types.h"

namespace ui_shared {
namespace behavior {

class AnalyzerBase;

class BehaviorAnalyzer : public QObject {
    Q_OBJECT
public:
    explicit BehaviorAnalyzer(QObject* parent = nullptr);
    ~BehaviorAnalyzer() override;

    // 内置分析器
    AnalysisResult analyzeFrequency(const QDateTime& start, const QDateTime& end);
    AnalysisResult analyzeModule(const QDateTime& start, const QDateTime& end);
    AnalysisResult analyzeTime(const QDateTime& start, const QDateTime& end);
    AnalysisResult analyzeInput(const QDateTime& start, const QDateTime& end);
    AnalysisResult analyzeHeatmap(const QDateTime& start, const QDateTime& end);
    AnalysisResult analyzeDialog(const QDateTime& start, const QDateTime& end);

    // 自定义分析器
    AnalysisResult analyze(const QString& name, const QDateTime& start, const QDateTime& end);

    // 导出原始数据
    QList<Operation> exportRawData(const QDateTime& start, const QDateTime& end);

private:
    QList<Operation> fetchData(const QDateTime& start, const QDateTime& end,
                               bool onlyMainWindow = false);
    AnalysisResult run(const QString& name, const QDateTime& start, const QDateTime& end,
                       bool onlyMainWindow = false);

    std::unique_ptr<AnalyzerBase> frequency_;
    std::unique_ptr<AnalyzerBase> module_;
    std::unique_ptr<AnalyzerBase> time_;
    std::unique_ptr<AnalyzerBase> input_;
    std::unique_ptr<AnalyzerBase> heatmap_;
    std::unique_ptr<AnalyzerBase> dialog_;
};

} // namespace behavior
} // namespace ui_shared
