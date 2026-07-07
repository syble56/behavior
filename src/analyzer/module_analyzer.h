#pragma once
#include "analyzer_base.h"

namespace ui_shared { namespace behavior {

class ModuleAnalyzer : public AnalyzerBase {
public:
    QString name() const override { return "module"; }
    AnalysisResult analyze(const QList<Operation>& ops) override;
private:
    QString extractModuleName(const QString& windowClass);
};

}} // namespace
