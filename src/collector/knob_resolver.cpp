#include "knob_resolver.h"
#include <QKeyEvent>

namespace ui_shared {
namespace behavior {

KnobResolver& KnobResolver::instance() {
    static KnobResolver inst;
    return inst;
}

void KnobResolver::loadDefaults() {
    mappings_.clear();
    mappings_.append({QKeySequence(Qt::CTRL | Qt::Key_F11), KnobAction::Left});
    mappings_.append({QKeySequence(Qt::CTRL | Qt::Key_F12), KnobAction::Right});
}

void KnobResolver::addMapping(const QKeySequence& seq, KnobAction action) {
    for (auto& m : mappings_) {
        if (m.sequence == seq) {
            m.action = action;
            return;
        }
    }
    mappings_.append({seq, action});
}

void KnobResolver::removeMapping(const QKeySequence& seq) {
    for (int i = 0; i < mappings_.size(); ++i) {
        if (mappings_[i].sequence == seq) {
            mappings_.removeAt(i);
            return;
        }
    }
}

void KnobResolver::clear() {
    mappings_.clear();
}

KnobResolver::KnobInfo KnobResolver::resolve(QKeyEvent* event) {
    KnobInfo info;
    if (!event) return info;

    int key = event->key();
    if (key == Qt::Key_Shift || key == Qt::Key_Control ||
        key == Qt::Key_Alt   || key == Qt::Key_Meta) return info;

    QKeySequence seq(key | event->modifiers());
    for (const auto& m : mappings_) {
        if (m.sequence.matches(seq) == QKeySequence::ExactMatch) {
            info.action = m.action;
            info.keySequence = seq.toString();
            info.valid = true;
            emit knobDetected(m.action);
            return info;
        }
    }
    return info;
}

} // namespace behavior
} // namespace ui_shared
