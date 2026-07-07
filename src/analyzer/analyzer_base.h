#pragma once

#include <QDateTime>
#include <QList>
#include "core/types.h"

namespace ui_shared {
namespace behavior {

class AnalyzerBase {
public:
    virtual ~AnalyzerBase() = default;

    virtual QString name() const = 0;
    virtual AnalysisResult analyze(const QList<Operation>& ops) = 0;

    void setTimeRange(const QDateTime& start, const QDateTime& end) {
        startTime_ = start;
        endTime_ = end;
    }

protected:
    QDateTime startTime_;
    QDateTime endTime_;
};

} // namespace behavior
} // namespace ui_shared
