#include "InputAnalyzer.h"
#include <QVariantMap>

namespace ui_shared {
namespace behavior {

AnalysisResult InputAnalyzer::analyze(const QList<Operation>& ops) {
    AnalysisResult r;
    if (ops.isEmpty()) { r.error = QStringLiteral("数据不足"); return r; }
    int mouse = 0, touch = 0, keyboard = 0;
    for (const auto& op : ops) {
        switch (op.inputMethod) {
            case InputMethod::Mouse:    mouse++; break;
            case InputMethod::Touch:    touch++; break;
            case InputMethod::Keyboard: keyboard++; break;
        }
    }
    int total = mouse + touch + keyboard;
    auto pct = [total](int c){ return (total > 0) ? (c * 100.0 / total) : 0.0; };

    QVariantMap m, t, k;
    m["count"] = mouse;     m["percentage"] = pct(mouse);
    t["count"] = touch;     t["percentage"] = pct(touch);
    k["count"] = keyboard;  k["percentage"] = pct(keyboard);
    r.data["total"] = total;
    r.data["mouse"] = m;
    r.data["touch"] = t;
    r.data["keyboard"] = k;
    r.success = true;
    return r;
}

}} // namespace
