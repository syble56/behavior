#pragma once

#include <QDateTime>
#include <QList>
#include "core/Types.h"

namespace ui_shared {
namespace behavior {

class AnalyzerBase {
public:
    virtual ~AnalyzerBase() = default;

    virtual QString name() const = 0;
    virtual AnalysisResult analyze(const QList<Operation>& ops) = 0;

    void setTimeRange(const QDateTime& start, const QDateTime& end) {
        m_startTime = start;
        m_endTime = end;
    }

protected:
    QDateTime m_startTime;
    QDateTime m_endTime;
};

} // namespace behavior
} // namespace ui_shared
