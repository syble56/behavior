#pragma once
#include "AnalyzerBase.h"

namespace ui_shared { namespace behavior {

class TimeAnalyzer : public AnalyzerBase {
public:
    enum class Granularity { Hour, Day, DayOfWeek };
    QString name() const override { return "time"; }
    AnalysisResult analyze(const QList<Operation>& ops) override;
    void setGranularity(Granularity g) { m_granularity = g; }
private:
    Granularity m_granularity = Granularity::Hour;
};

}} // namespace
