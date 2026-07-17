#include "heatmap_analyzer.h"
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>

namespace ui_shared {
namespace behavior {

QList<Operation> HeatmapAnalyzer::filterMainWindow(const QList<Operation>& ops) {
    QList<Operation> out;
    for (const auto& op : ops) {
        if (op.isMainWindow) out.append(op);
    }
    return out;
}

AnalysisResult HeatmapAnalyzer::analyze(const QList<Operation>& ops) {
    AnalysisResult r;
    auto main = filterMainWindow(ops);
    if (main.isEmpty()) { r.error = QStringLiteral("无主窗口点击数据"); return r; }

    QHash<int, int> counts;  // region -> count
    int maxCount = 0;
    for (const auto& op : main) {
        if (op.heatRegion < 0 || op.heatRegion > 99) continue;
        int c = ++counts[op.heatRegion];
        if (c > maxCount) maxCount = c;
    }

    QVariantList regions;
    auto keys = counts.keys();
    std::sort(keys.begin(), keys.end());
    for (int reg : keys) {
        QVariantMap item;
        item["region"] = reg;
        item["row"] = reg / 10;
        item["col"] = reg % 10;
        item["count"] = counts[reg];
        regions.append(item);
    }
    r.data["grid_size"] = QVariantList{10, 10};
    r.data["regions"] = regions;
    r.data["max_count"] = maxCount;
    r.success = true;
    return r;
}

}} // namespace
