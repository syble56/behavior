#include "event_filter.h"
#include "event_processor.h"
#include "control_inspector.h"
#include "core/config.h"
#include <QApplication>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QKeyEvent>
#include <QWidget>
#include <QEvent>
#include <QDialog>
#include <QDebug>

namespace ui_shared {
namespace behavior {

EventFilter::EventFilter(EventProcessor* processor, QObject* parent)
    : QObject(parent), processor_(processor) {}

EventFilter::~EventFilter() {
    uninstall();
}

void EventFilter::install() {
    if (qApp) qApp->installEventFilter(this);
}

void EventFilter::uninstall() {
    if (qApp) qApp->removeEventFilter(this);
}

bool EventFilter::eventFilter(QObject* watched, QEvent* event) {
    if (!Config::instance().enabled()) return false;
    
    // 对话框事件不需要 spontaneous 检查
    if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && w->isTopLevel() && qobject_cast<QDialog*>(w)) {
            if (event->type() == QEvent::Show) {
                processor_->processDialogOpen(watched);
                lastDialogGeometry_ = QRect();  // 清除记录
            } else {
                // 记录对话框关闭前的几何区域（包含边框和标题栏）
                lastDialogGeometry_ = w->frameGeometry();
                processor_->processDialogClose(watched);
            }
            return false;
        }
    }
    
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (!event->isSpontaneous()) return false;
#else
    if (!event->spontaneous()) return false;
#endif

    switch (event->type()) {
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QPoint globalPos = me->globalPosition().toPoint();
#else
            QPoint globalPos = me->globalPos();
#endif
            // 如果点击位置在刚关闭的对话框区域内，跳过
            if (lastDialogGeometry_.isValid() && 
                lastDialogGeometry_.contains(globalPos)) {
                lastDialogGeometry_ = QRect();  // 清除记录，只跳过一次
                break;
            }
            
            QWidget* target = QApplication::widgetAt(globalPos);
            if (!target) break;
            if (ControlInspector::shouldIgnore(target) &&
                !ControlInspector::isAreaClick(target)) break;
            processor_->processMouseClick(target, globalPos);
            break;
        }
        case QEvent::TouchEnd: {
            auto* te = static_cast<QTouchEvent*>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            const auto& points = te->points();
            if (points.isEmpty()) break;
            QPoint globalPos = points.last().globalPosition().toPoint();
#else
            const auto& points = te->touchPoints();
            if (points.isEmpty()) break;
            QPoint globalPos = points.last().screenPos().toPoint();
#endif
            QWidget* target = QApplication::widgetAt(globalPos);
            if (!target) break;
            processor_->processTouchTap(target, globalPos);
            break;
        }
        case QEvent::KeyPress: {
            auto* ke = static_cast<QKeyEvent*>(event);
            int key = ke->key();
            // Esc 和 Enter 无论是否带修饰键都记录
            if (key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter) {
                processor_->processKeyPress(qobject_cast<QWidget*>(watched), ke);
                break;
            }
            // 仅带修饰键的按键视为快捷键
            if (ke->modifiers() == Qt::NoModifier) break;
            processor_->processShortcut(qobject_cast<QWidget*>(watched), ke);
            break;
        }
        default:
            break;
    }
    return false;  // 不拦截事件
}

} // namespace behavior
} // namespace ui_shared
