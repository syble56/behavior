#pragma once
#include "AnalyzerBase.h"

namespace ui_shared { namespace behavior {

class HeatmapAnalyzer : public AnalyzerBase {
public:
    QString name() const override { return "heatmap"; }
    AnalysisResult analyze(const QList<Operation>& ops) override;
private:
    QList<Operation> filterMainWindow(const QList<Operation>& ops);
};

}} // namespace
