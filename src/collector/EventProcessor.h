#pragma once

#include <QObject>
#include <QPoint>
#include <QString>
#include <functional>
#include <memory>
#include "core/Types.h"

class QKeyEvent;
class QWidget;

namespace ui_shared {
namespace behavior {

class OperationQueue;
class ControlInspector;
class ShortcutResolver;
class DialogTracker;

class EventProcessor : public QObject {
    Q_OBJECT
public:
    explicit EventProcessor(OperationQueue* queue);
    ~EventProcessor();

    void setSessionId(const QString& sessionId) { m_sessionId = sessionId; }

    // 处理鼠标点击（globalPos 为全局坐标）
    void processMouseClick(QWidget* target, const QPoint& globalPos);
    // 处理触屏点击
    void processTouchTap(QWidget* target, const QPoint& globalPos);
    // 处理快捷键
    void processShortcut(QWidget* target, QKeyEvent* event);
    // 处理对话框打开
    void processDialogOpen(QObject* dialog);
    // 处理对话框关闭
    void processDialogClose(QObject* dialog);

signals:
    void operationRecorded(const QString& logMessage);

private:
    Operation createOperation(QWidget* target, EventType type, InputMethod method);
    void enqueue(Operation&& op);
    bool shouldDedup(const QPoint& globalPos, EventType type, qint64 ts);

    OperationQueue* m_queue;
    std::unique_ptr<DialogTracker> m_dialogTracker;
    QString m_sessionId;

    // 去重
    QPoint m_lastPos;
    EventType m_lastType = EventType::MouseClick;
    qint64 m_lastTs = 0;
};

} // namespace behavior
} // namespace ui_shared
