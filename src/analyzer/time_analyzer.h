#pragma once
#include "analyzer_base.h"

namespace ui_shared { namespace behavior {

class TimeAnalyzer : public AnalyzerBase {
public:
    enum class Granularity { Hour, Day, DayOfWeek };
    QString name() const override { return "time"; }
    AnalysisResult analyze(const QList<Operation>& ops) override;
    void setGranularity(Granularity g) { granularity_ = g; }
private:
    Granularity granularity_ = Granularity::Hour;
};

}} // namespace
