#include "Session.h"
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
    m_current = Session{};
    m_current.id = generateId();
    m_current.startTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    m_active = true;
    return m_current.id;
}

Session SessionManager::end() {
    if (!m_active) return m_current;
    m_current.endTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    m_current.durationSeconds = static_cast<int>((m_current.endTime - m_current.startTime) / 1000);
    m_active = false;
    return m_current;
}

} // namespace behavior
} // namespace ui_shared
