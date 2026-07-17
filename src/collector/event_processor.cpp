#include "event_processor.h"
#include "control_inspector.h"
#include "shortcut_resolver.h"
#include "dialog_tracker.h"
#include "storage/operation_queue.h"
#include "storage/database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QApplication>
#include <QWidget>
#include <QDateTime>
#include <QKeyEvent>
#include <QKeySequence>

namespace ui_shared {
namespace behavior {

EventProcessor::EventProcessor(OperationQueue* queue)
    : queue_(queue), dialogTracker_(std::make_unique<DialogTracker>()) {}

EventProcessor::~EventProcessor() = default;

bool EventProcessor::shouldDedup(const QPoint& globalPos, EventType type, qint64 ts) {
    if (lastPos_ == globalPos && lastType_ == type &&
        ts - lastTs_ >= 0 && ts - lastTs_ < 50) {
        return true;
    }
    lastPos_ = globalPos;
    lastType_ = type;
    lastTs_ = ts;
    return false;
}

Operation EventProcessor::createOperation(QWidget* target, EventType type, InputMethod method) {
    Operation op;
    op.sessionId = sessionId_;
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
    op.module = module_;
    return op;
}

void EventProcessor::enqueue(Operation&& op) {
    if (queue_) queue_->enqueue(std::move(op));
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
    dialogTracker_->onDialogOpen(dialog);
    Operation op = createOperation(qobject_cast<QWidget*>(dialog),
                                   EventType::DialogOpen, InputMethod::Derived);
    enqueue(std::move(op));
    
    QString log = QString("[%1] DialogOpen: %2")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(ControlInspector::className(dialog));
    emit operationRecorded(log);
}

void EventProcessor::processDialogClose(QObject* dialog) {
    if (!dialog) return;
    int dur = dialogTracker_->onDialogClose(dialog);
    Operation op = createOperation(qobject_cast<QWidget*>(dialog),
                                   EventType::DialogClose, InputMethod::Derived);
    if (dur > 0) op.duration = dur;
    enqueue(std::move(op));
    
    QString log = QString("[%1] DialogClose: %2 (duration: %3ms)")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(ControlInspector::className(dialog))
        .arg(dur);
    emit operationRecorded(log);
}

void EventProcessor::recoverUnclosedDialogs() {
    // 查找有 dialog_open 但没有对应 dialog_close 的记录
    auto& db = Database::instance();
    if (!db.isOpen()) return;
    QSqlDatabase sqlDb = db.connection();
    if (!sqlDb.isOpen()) return;

    QSqlQuery q(sqlDb);
    // 1. 先取最近 20 条 dialog_open（走 idx_ops_event 索引，秒级）
    q.prepare("SELECT id, session_id, time, window_class, window_title, window_path, module "
              "FROM operations WHERE event_type = 'dialog_open' ORDER BY time DESC LIMIT 20");
    q.exec();

    struct OpenDialog {
        qint64 id;
        QString sessionId;
        qint64 openTime;
        QString windowClass;
        QString windowTitle;
        QString windowPath;
        QString module;
    };
    QVector<OpenDialog> candidates;
    while (q.next()) {
        candidates.append({
            q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toLongLong(),
            q.value(3).toString(), q.value(4).toString(), q.value(5).toString(), q.value(6).toString()
        });
    }

    if (candidates.isEmpty()) return;

    // 2. 逐条检查是否有对应的 dialog_close
    qint64 now = QDateTime::currentDateTime().toMSecsSinceEpoch();
    int recovered = 0;
    for (const auto& dlg : candidates) {
        QSqlQuery q2(sqlDb);
        q2.prepare("SELECT 1 FROM operations WHERE event_type = 'dialog_close' "
                   "AND window_class = ? AND COALESCE(module,'') = COALESCE(?,'') AND time > ? LIMIT 1");
        q2.addBindValue(dlg.windowClass);
        q2.addBindValue(dlg.module);
        q2.addBindValue(dlg.openTime);
        q2.exec();
        if (q2.next()) continue;  // 已关闭，跳过

        // 未关闭，生成补丁 close 事件
        Operation op;
        op.sessionId = dlg.sessionId;
        op.timestamp = now;
        op.eventType = EventType::DialogClose;
        op.inputMethod = InputMethod::Derived;
        op.windowClass = dlg.windowClass;
        op.windowTitle = dlg.windowTitle;
        op.windowPath = dlg.windowPath;
        op.module = dlg.module;
        int dur = static_cast<int>(now - dlg.openTime);
        if (dur > 0) op.duration = dur;
        enqueue(std::move(op));
        ++recovered;
    }

    if (recovered > 0) {
        qDebug("[Behavior] Recovered %d unclosed dialogs", recovered);
        emit operationRecorded(QString("恢复 %1 个未正常关闭的弹窗").arg(recovered));
    }
}

void EventProcessor::processKeyPress(QWidget* target, QKeyEvent* event) {
    if (!target || !event) return;
    int key = event->key();
    Operation op = createOperation(target, EventType::Shortcut, InputMethod::Keyboard);
    op.keySequence = QKeySequence(key).toString();
    // 标记动作名
    if (key == Qt::Key_Escape) {
        op.actionName = "escape";
    } else if (key == Qt::Key_Return) {
        op.actionName = "enter";
    } else if (key == Qt::Key_Enter) {
        op.actionName = "enter";
    }
    enqueue(std::move(op));

    QString log = QString("[%1] KeyPress: %2")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(QKeySequence(key).toString());
    emit operationRecorded(log);
}

} // namespace behavior
} // namespace ui_shared
