#include "shortcut_resolver.h"
#include <QKeyEvent>
#include <QKeySequence>
#include <QAction>
#include <QWidget>
#include <QMenu>
#include <QMenuBar>

namespace ui_shared {
namespace behavior {

QList<QAction*> ShortcutResolver::getAllActions(QWidget* widget) {
    QList<QAction*> all = widget->actions();
    // 菜单栏中的动作
    if (auto* mb = widget->findChild<QMenuBar*>()) {
        for (QMenu* m : mb->findChildren<QMenu*>()) {
            all.append(m->actions());
        }
    }
    // 子菜单
    for (QMenu* m : widget->findChildren<QMenu*>()) {
        all.append(m->actions());
    }
    return all;
}

QAction* ShortcutResolver::findAction(QWidget* widget, const QKeySequence& seq) {
    if (!widget || seq.isEmpty()) return nullptr;
    QList<QAction*> actions = getAllActions(widget);
    for (QAction* a : actions) {
        if (!a->shortcut().isEmpty() && a->shortcut().matches(seq) == QKeySequence::ExactMatch)
            return a;
    }
    return nullptr;
}

ShortcutResolver::ShortcutInfo ShortcutResolver::resolve(QObject* target, QKeyEvent* event) {
    ShortcutInfo info;
    if (!target || !event) return info;

    int key = event->key();
    if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
        key == Qt::Key_Meta) return info;

    Qt::KeyboardModifiers mods = event->modifiers();
    QKeySequence seq(key | mods);
    info.keySequence = seq.toString();

    auto* w = qobject_cast<QWidget*>(target);
    if (!w) w = qobject_cast<QWidget*>(target->parent());
    if (!w) return info;

    QAction* a = findAction(w, seq);
    if (a) {
        info.actionName = a->text().isEmpty() ? a->objectName() : a->text();
        info.valid = true;
    } else {
        // 无匹配QAction，但仍记录为快捷键事件
        info.valid = true;
    }
    return info;
}

} // namespace behavior
} // namespace ui_shared
