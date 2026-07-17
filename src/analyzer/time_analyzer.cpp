#include "time_analyzer.h"
#include <QDateTime>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>

namespace ui_shared {
namespace behavior {

AnalysisResult TimeAnalyzer::analyze(const QList<Operation>& ops) {
    AnalysisResult r;
    if (ops.isEmpty()) { r.error = QStringLiteral("数据不足"); return r; }

    QHash<int, int> counts;
    for (const auto& op : ops) {
        QDateTime t = QDateTime::fromMSecsSinceEpoch(op.timestamp);
        int bucket = 0;
        switch (granularity_) {
            case Granularity::Hour:      bucket = t.time().hour(); break;
            case Granularity::Day:       bucket = t.date().dayOfYear(); break;
            case Granularity::DayOfWeek: bucket = static_cast<int>(t.date().dayOfWeek()); break;
        }
        counts[bucket]++;
    }

    QList<int> keys = counts.keys();
    std::sort(keys.begin(), keys.end());

    QVariantList by;
    int maxCount = 0;
    QList<int> peaks;
    for (int k : keys) {
        QVariantMap item;
        item["bucket"] = k;
        item["count"] = counts[k];
        by.append(item);
        if (counts[k] > maxCount) {
            maxCount = counts[k];
            peaks.clear();
            peaks.append(k);
        } else if (counts[k] == maxCount) {
            peaks.append(k);
        }
    }
    if (granularity_ == Granularity::Hour) r.data["by_hour"] = by;
    else r.data["by_bucket"] = by; {
        r.data["peak_buckets"] = QVariant::fromValue(peaks);
    }
    r.success = true;
    return r;
}

}} // namespace
