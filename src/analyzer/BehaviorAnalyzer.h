#pragma once

#include <QObject>
#include <QDateTime>
#include <memory>
#include "core/Types.h"

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

    std::unique_ptr<AnalyzerBase> m_frequency;
    std::unique_ptr<AnalyzerBase> m_module;
    std::unique_ptr<AnalyzerBase> m_time;
    std::unique_ptr<AnalyzerBase> m_input;
    std::unique_ptr<AnalyzerBase> m_heatmap;
    std::unique_ptr<AnalyzerBase> m_dialog;
};

} // namespace behavior
} // namespace ui_shared
