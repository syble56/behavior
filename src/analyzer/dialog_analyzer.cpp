#include "dialog_analyzer.h"
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <limits>

namespace ui_shared {
namespace behavior {

static const int INSTANT_CLOSE_THRESHOLD_MS = 500;

AnalysisResult DialogAnalyzer::analyze(const QList<Operation>& ops) {
    AnalysisResult r;
    if (ops.isEmpty()) { r.error = QStringLiteral("数据不足"); return r; }

    QHash<QString, DialogStats> stats;
    int totalOpens = 0;
    for (const auto& op : ops) {
        // 标识优先级：windowTitle > controlName > windowClass
        QString dialogKey;
        if (!op.windowTitle.isEmpty()) {
            dialogKey = op.windowTitle;
        }
        else if (!op.controlName.isEmpty()) {
            dialogKey = op.controlName;
        }
        else {
            dialogKey = op.windowClass;
        }
        // 加上 module 前缀
        if (!op.module.isEmpty()) {
            dialogKey = op.module + "/" + dialogKey;
        }
        
        if (op.eventType == EventType::DialogOpen) {
            stats[dialogKey].openCount++;
            ++totalOpens;
        } else if (op.eventType == EventType::DialogClose && op.duration.has_value()) {
            int dur = *op.duration;
            auto& s = stats[dialogKey];
            s.totalDuration += dur;
            if (s.minDuration == 0 || dur < s.minDuration) s.minDuration = dur;
            if (dur > s.maxDuration) s.maxDuration = dur;
            s.durations.append(dur);
            if (dur < INSTANT_CLOSE_THRESHOLD_MS) s.instantCloseCount++;
        }
    }

    QVariantList dialogs;
    auto keys = stats.keys();
    std::sort(keys.begin(), keys.end());
    for (const QString& cls : keys) {
        const auto& s = stats[cls];
        int closeCount = s.durations.size();
        int avg = (closeCount > 0) ? static_cast<int>(s.totalDuration / closeCount) : 0;

        // 中位数
        int median = 0;
        double instantCloseRate = 0.0;
        if (closeCount > 0) {
            QVector<int> sorted = s.durations;
            std::sort(sorted.begin(), sorted.end());
            if (sorted.size() % 2 == 1) {
                median = sorted[sorted.size() / 2];
            }
            else {
                median = (sorted[sorted.size() / 2 - 1] + sorted[sorted.size() / 2]) / 2;
            }
            instantCloseRate = s.instantCloseCount * 100.0 / closeCount;
        }

        QVariantMap item;
        item["class"] = cls;
        item["open_count"] = s.openCount;
        item["close_count"] = closeCount;
        item["avg_duration_ms"] = avg;
        item["median_duration_ms"] = median;
        item["min_duration_ms"] = s.minDuration;
        item["max_duration_ms"] = s.maxDuration;
        item["instant_close_rate"] = QString::number(instantCloseRate, 'f', 1) + "%";
        dialogs.append(item);
    }
    r.data["total_opens"] = totalOpens;
    r.data["dialogs"] = dialogs;
    r.success = true;
    return r;
}

}} // namespace
