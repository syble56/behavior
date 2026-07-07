#include "dialog_tracker.h"
#include <QDateTime>
#include <QWidget>

namespace ui_shared {
namespace behavior {

DialogTracker::DialogTracker() = default;

QString DialogTracker::dialogId(QObject* dialog) {
    if (!dialog) return {};
    return QString::fromLatin1(dialog->metaObject()->className()) + "/" + dialog->objectName();
}

void DialogTracker::onDialogOpen(QObject* dialog) {
    openTimes_.insert(dialogId(dialog), QDateTime::currentDateTime().toMSecsSinceEpoch());
}

int DialogTracker::onDialogClose(QObject* dialog) {
    QString id = dialogId(dialog);
    auto it = openTimes_.find(id);
    if (it == openTimes_.end()) return 0;
    qint64 opened = it.value();
    openTimes_.erase(it);
    return static_cast<int>(QDateTime::currentDateTime().toMSecsSinceEpoch() - opened);
}

} // namespace behavior
} // namespace ui_shared
