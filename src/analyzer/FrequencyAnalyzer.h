#pragma once
#include "AnalyzerBase.h"

namespace ui_shared { namespace behavior {

class FrequencyAnalyzer : public AnalyzerBase {
public:
    QString name() const override { return "frequency"; }
    AnalysisResult analyze(const QList<Operation>& ops) override;
    void setTopCount(int count) { m_topCount = count; }
private:
    int m_topCount = 10;
};

}} // namespace
