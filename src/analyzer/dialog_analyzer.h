#pragma once
#include "analyzer_base.h"
#include <QHash>
#include <QList>
#include <QVector>

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
        QVector<int> durations;      // 每次关闭时长，用于算中位数和立即关闭率
        int instantCloseCount = 0;   // <500ms 关闭次数
    };
};

}} // namespace
