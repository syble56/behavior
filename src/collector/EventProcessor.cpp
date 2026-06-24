#include "EventProcessor.h"
#include "ControlInspector.h"
#include "ShortcutResolver.h"
#include "DialogTracker.h"
#include "storage/OperationQueue.h"
#include <QApplication>
#include <QWidget>
#include <QDateTime>
#include <QKeyEvent>

namespace ui_shared {
namespace behavior {

EventProcessor::EventProcessor(OperationQueue* queue)
    : m_queue(queue), m_dialogTracker(std::make_unique<DialogTracker>()) {}

EventProcessor::~EventProcessor() = default;

bool EventProcessor::shouldDedup(const QPoint& globalPos, EventType type, qint64 ts) {
    if (m_lastPos == globalPos && m_lastType == type &&
        ts - m_lastTs >= 0 && ts - m_lastTs < 50) {
        return true;
    }
    m_lastPos = globalPos;
    m_lastType = type;
    m_lastTs = ts;
    return false;
}

Operation EventProcessor::createOperation(QWidget* target, EventType type, InputMethod method) {
    Operation op;
    op.sessionId = m_sessionId;
    op.timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    op.eventType = type;
    op.inputMethod = method;

    if (target) {
        op.controlClass = ControlInspector::className(target);
        op.controlName = ControlInspector::objectName(target);
        op.controlText = ControlInspector::displayText(target);
        op.controlPath = ControlInspector::path(target);
        op.windowClass = ControlInspector::windowClassName(target);
        op.windowTitle = ControlInspector::windowTitle(target);
        op.windowPath = ControlInspector::windowPath(target);
        op.isMainWindow = ControlInspector::isMainWindow(target);
    }
    return op;
}

void EventProcessor::enqueue(Operation&& op) {
    if (m_queue) m_queue->enqueue(std::move(op));
}

void EventProcessor::processMouseClick(QWidget* target, const QPoint& globalPos) {
    if (!target) return;
    qint64 ts = QDateTime::currentDateTime().toMSecsSinceEpoch();
    if (shouldDedup(globalPos, EventType::MouseClick, ts)) return;

    bool area = ControlInspector::isAreaClick(target);
    EventType type = area ? EventType::AreaClick : EventType::MouseClick;
    Operation op = createOperation(target, type, InputMethod::Mouse);
    op.screenX = globalPos.x();
    op.screenY = globalPos.y();
    QWidget* win = ControlInspector::window(target);
    op.heatRegion = ControlInspector::heatRegion(globalPos, win);
    enqueue(std::move(op));
    
    QString log = QString("[%1] %2 at (%3,%4) - %5")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(area ? "AreaClick" : "MouseClick")
        .arg(globalPos.x()).arg(globalPos.y())
        .arg(ControlInspector::className(target));
    emit operationRecorded(log);
}

void EventProcessor::processTouchTap(QWidget* target, const QPoint& globalPos) {
    if (!target) return;
    qint64 ts = QDateTime::currentDateTime().toMSecsSinceEpoch();
    if (shouldDedup(globalPos, EventType::TouchTap, ts)) return;

    bool area = ControlInspector::isAreaClick(target);
    EventType type = area ? EventType::AreaClick : EventType::TouchTap;
    Operation op = createOperation(target, type, InputMethod::Touch);
    op.screenX = globalPos.x();
    op.screenY = globalPos.y();
    QWidget* win = ControlInspector::window(target);
    op.heatRegion = ControlInspector::heatRegion(globalPos, win);
    enqueue(std::move(op));
    
    QString log = QString("[%1] TouchTap at (%2,%3) - %4")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(globalPos.x()).arg(globalPos.y())
        .arg(ControlInspector::className(target));
    emit operationRecorded(log);
}

void EventProcessor::processShortcut(QWidget* target, QKeyEvent* event) {
    auto info = ShortcutResolver::resolve(target, event);
    if (!info.valid) return;
    Operation op = createOperation(qobject_cast<QWidget*>(target),
                                   EventType::Shortcut, InputMethod::Keyboard);
    op.actionName = info.actionName;
    op.keySequence = info.keySequence;
    enqueue(std::move(op));
    
    QString log = QString("[%1] Shortcut: %2 (%3)")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(info.actionName.isEmpty() ? "unknown" : info.actionName)
        .arg(info.keySequence);
    emit operationRecorded(log);
}

void EventProcessor::processDialogOpen(QObject* dialog) {
    if (!dialog) return;
    m_dialogTracker->onDialogOpen(dialog);
    Operation op = createOperation(qobject_cast<QWidget*>(dialog),
                                   EventType::DialogOpen, InputMethod::Mouse);
    enqueue(std::move(op));
    
    QString log = QString("[%1] DialogOpen: %2")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(ControlInspector::className(dialog));
    emit operationRecorded(log);
}

void EventProcessor::processDialogClose(QObject* dialog) {
    if (!dialog) return;
    int dur = m_dialogTracker->onDialogClose(dialog);
    Operation op = createOperation(qobject_cast<QWidget*>(dialog),
                                   EventType::DialogClose, InputMethod::Mouse);
    if (dur > 0) op.duration = dur;
    enqueue(std::move(op));
    
    QString log = QString("[%1] DialogClose: %2 (duration: %3ms)")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(ControlInspector::className(dialog))
        .arg(dur);
    emit operationRecorded(log);
}

} // namespace behavior
} // namespace ui_shared
