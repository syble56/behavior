#pragma once

#include <QObject>
#include <QPoint>
#include <QString>
#include <functional>
#include <memory>
#include "core/types.h"

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

    void setSessionId(const QString& sessionId) { sessionId_ = sessionId; }
    void setModule(const QString& module) { module_ = module; }

    // 处理鼠标点击（globalPos 为全局坐标）
    void processMouseClick(QWidget* target, const QPoint& globalPos);
    // 处理触屏点击
    void processTouchTap(QWidget* target, const QPoint& globalPos);
    // 处理快捷键
    void processShortcut(QWidget* target, QKeyEvent* event);
    // 处理独立按键（Esc/Enter）
    void processKeyPress(QWidget* target, QKeyEvent* event);
    // 处理旋钮事件
    void processKnob(QWidget* target, QKeyEvent* event);
    // 处理对话框打开
    void processDialogOpen(QObject* dialog);
    // 处理对话框关闭
    void processDialogClose(QObject* dialog);
    // 补齐上次未正常关闭的弹窗（启动时调用）
    void recoverUnclosedDialogs();

signals:
    void operationRecorded(const QString& logMessage);

private:
    Operation createOperation(QWidget* target, EventType type, InputMethod method);
    void enqueue(Operation&& op);
    bool shouldDedup(const QPoint& globalPos, EventType type, qint64 ts);

    OperationQueue* queue_;
    std::unique_ptr<DialogTracker> dialogTracker_;
    QString sessionId_;
    QString module_;

    // 去重
    QPoint lastPos_;
    EventType lastType_ = EventType::MouseClick;
    qint64 lastTs_ = 0;
};

} // namespace behavior
} // namespace ui_shared
