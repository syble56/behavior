#pragma once

#include <QObject>
#include <QKeySequence>
#include <QVector>
#include <QString>

class QKeyEvent;

namespace ui_shared {
namespace behavior {

// 旋钮动作类型
enum class KnobAction {
    Left,   // 左旋
    Right   // 右旋
};

inline const char* knobActionToString(KnobAction a) {
    switch (a) {
        case KnobAction::Left:  return "knob_left";
        case KnobAction::Right: return "knob_right";
    }
    return "knob_unknown";
}

// 旋钮按键映射器：将特定组合键解析为旋钮动作
// 默认映射: Ctrl+F11 → 左旋, Ctrl+F12 → 右旋
class KnobResolver : public QObject {
    Q_OBJECT
public:
    struct KnobInfo {
        KnobAction action;
        QString keySequence;     // "Ctrl+F11"
        bool valid = false;
    };

    static KnobResolver& instance();

    // 加载默认映射 (Ctrl+F11→Left, Ctrl+F12→Right)
    void loadDefaults();

    // 自定义映射
    void addMapping(const QKeySequence& seq, KnobAction action);
    void removeMapping(const QKeySequence& seq);
    void clear();

    // 解析按键事件，若命中旋钮映射则返回 KnobInfo{valid=true} 并发射 knobDetected 信号
    KnobInfo resolve(QKeyEvent* event);

signals:
    void knobDetected(KnobAction action);

private:
    KnobResolver() = default;

    struct Mapping {
        QKeySequence sequence;
        KnobAction action;
    };
    QVector<Mapping> mappings_;
};

} // namespace behavior
} // namespace ui_shared
