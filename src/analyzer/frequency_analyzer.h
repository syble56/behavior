#pragma once
#include "analyzer_base.h"

namespace ui_shared { namespace behavior {

class FrequencyAnalyzer : public AnalyzerBase {
public:
    QString name() const override { return "frequency"; }
    AnalysisResult analyze(const QList<Operation>& ops) override;
    void setTopCount(int count) { topCount_ = count; }
private:
    int topCount_ = 10;
};

}} // namespace
