#include "frequency_analyzer.h"
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>

namespace ui_shared {
namespace behavior {

AnalysisResult FrequencyAnalyzer::analyze(const QList<Operation>& ops) {
    AnalysisResult r;
    if (ops.isEmpty()) {
        r.error = QStringLiteral("数据不足，无法分析");
        return r;
    }
    QHash<QString, int> counts;
    for (const auto& op : ops) {
        QString key = op.actionName.isEmpty() ? op.controlName : op.actionName;
        if (key.isEmpty()) key = op.controlClass;
        if (key.isEmpty()) continue;
        counts[key]++;
    }
    int total = 0;
    for (auto it = counts.begin(); it != counts.end(); ++it) total += it.value();

    QVector<QPair<QString,int>> vec;
    vec.reserve(counts.size());
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        vec.append({it.key(), it.value()});
    }
    std::sort(vec.begin(), vec.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });

    QVariantList top;
    int n = qMin(topCount_, vec.size());
    for (int i = 0; i < n; ++i) {
        QVariantMap item;
        item["action"] = vec[i].first;
        item["count"] = vec[i].second;
        item["percentage"] = (total > 0) ? (vec[i].second * 100.0 / total) : 0.0;
        top.append(item);
    }
    r.data["total_operations"] = total;
    r.data["unique_actions"] = counts.size();
    r.data["top_actions"] = top;
    r.success = true;
    return r;
}

}} // namespace
