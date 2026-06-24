#pragma once

#include <QObject>
#include <QString>
#include <QList>

class QKeyEvent;
class QAction;
class QWidget;
class QKeySequence;

namespace ui_shared {
namespace behavior {

class ShortcutResolver {
public:
    struct ShortcutInfo {
        QString actionName;      // QAction的文本
        QString keySequence;     // 如 "Ctrl+S"
        bool valid = false;
    };

    static ShortcutInfo resolve(QObject* target, QKeyEvent* event);

private:
    static QAction* findAction(QWidget* widget, const QKeySequence& seq);
    static QList<QAction*> getAllActions(QWidget* widget);
};

} // namespace behavior
} // namespace ui_shared
