#include "DialogAnalyzer.h"
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <limits>

namespace ui_shared {
namespace behavior {

AnalysisResult DialogAnalyzer::analyze(const QList<Operation>& ops) {
    AnalysisResult r;
    if (ops.isEmpty()) { r.error = QStringLiteral("数据不足"); return r; }

    QHash<QString, DialogStats> stats;
    int totalOpens = 0;
    for (const auto& op : ops) {
        if (op.eventType == EventType::DialogOpen) {
            stats[op.windowClass].openCount++;
            ++totalOpens;
        } else if (op.eventType == EventType::DialogClose && op.duration.has_value()) {
            auto& s = stats[op.windowClass];
            s.totalDuration += *op.duration;
            if (s.minDuration == 0 || *op.duration < s.minDuration) s.minDuration = *op.duration;
            if (*op.duration > s.maxDuration) s.maxDuration = *op.duration;
        }
    }

    QVariantList dialogs;
    auto keys = stats.keys();
    std::sort(keys.begin(), keys.end());
    for (const QString& cls : keys) {
        const auto& s = stats[cls];
        int avg = (s.openCount > 0) ? static_cast<int>(s.totalDuration / s.openCount) : 0;
        QVariantMap item;
        item["class"] = cls;
        item["open_count"] = s.openCount;
        item["avg_duration_ms"] = avg;
        item["min_duration_ms"] = s.minDuration;
        item["max_duration_ms"] = s.maxDuration;
        dialogs.append(item);
    }
    r.data["total_opens"] = totalOpens;
    r.data["dialogs"] = dialogs;
    r.success = true;
    return r;
}

}} // namespace
