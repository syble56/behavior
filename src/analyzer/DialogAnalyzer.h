#pragma once
#include "AnalyzerBase.h"
#include <QHash>
#include <QList>

namespace ui_shared { namespace behavior {

class DialogAnalyzer : public AnalyzerBase {
public:
    QString name() const override { return "dialog"; }
    AnalysisResult analyze(const QList<Operation>& ops) override;
private:
    struct DialogStats {
        int openCount = 0;
        qint64 totalDuration = 0;
        int minDuration = 0;
        int maxDuration = 0;
    };
};

}} // namespace
