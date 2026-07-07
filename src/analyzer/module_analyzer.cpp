#include "module_analyzer.h"
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>

namespace ui_shared {
namespace behavior {

QString ModuleAnalyzer::extractModuleName(const QString& windowClass) {
    if (windowClass.isEmpty()) return QStringLiteral("Unknown");
    QString m = windowClass;
    if (m.startsWith('Q')) m = m.mid(1);
    // 去掉常见的 Window/Dialog/Widget 后缀
    for (const QString& suffix : {"Dialog", "Window", "Widget", "Panel", "Page"}) {
        if (m.endsWith(suffix) && m.size() > suffix.size()) {
            m = m.left(m.size() - suffix.size());
            break;
        }
    }
    return m.isEmpty() ? windowClass : m;
}

AnalysisResult ModuleAnalyzer::analyze(const QList<Operation>& ops) {
    AnalysisResult r;
    if (ops.isEmpty()) { r.error = QStringLiteral("数据不足"); return r; }
    QHash<QString, int> counts;
    for (const auto& op : ops) {
        counts[extractModuleName(op.windowClass)]++;
    }
    int total = 0;
    for (auto it = counts.begin(); it != counts.end(); ++it) total += it.value();

    QVector<QPair<QString,int>> vec;
    for (auto it = counts.begin(); it != counts.end(); ++it)
        vec.append({it.key(), it.value()});
    std::sort(vec.begin(), vec.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });

    QVariantList modules;
    for (const auto& p : vec) {
        QVariantMap item;
        item["module"] = p.first;
        item["count"] = p.second;
        item["percentage"] = (total > 0) ? (p.second * 100.0 / total) : 0.0;
        modules.append(item);
    }
    r.data["total_modules"] = counts.size();
    r.data["modules"] = modules;
    r.success = true;
    return r;
}

}} // namespace
