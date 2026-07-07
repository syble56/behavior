#include "session.h"
#include <QUuid>
#include <QDateTime>

namespace ui_shared {
namespace behavior {

SessionManager::SessionManager(QObject* parent)
    : QObject(parent) {}

QString SessionManager::generateId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString SessionManager::start() {
    current_ = Session{};
    current_.id = generateId();
    current_.startTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    active_ = true;
    return current_.id;
}

Session SessionManager::end() {
    if (!active_) return current_;
    current_.endTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    current_.durationSeconds = static_cast<int>((current_.endTime - current_.startTime) / 1000);
    active_ = false;
    return current_;
}

} // namespace behavior
} // namespace ui_shared
