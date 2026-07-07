#pragma once
#include "analyzer_base.h"

namespace ui_shared { namespace behavior {

class InputAnalyzer : public AnalyzerBase {
public:
    QString name() const override { return "input"; }
    AnalysisResult analyze(const QList<Operation>& ops) override;
};

}} // namespace
