#include "EventFilter.h"
#include "EventProcessor.h"
#include "ControlInspector.h"
#include "core/Config.h"
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
    : QObject(parent), m_processor(processor) {}

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
                m_processor->processDialogOpen(watched);
                m_lastDialogGeometry = QRect();  // 清除记录
            } else {
                // 记录对话框关闭前的几何区域（包含边框和标题栏）
                m_lastDialogGeometry = w->frameGeometry();
                m_processor->processDialogClose(watched);
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
            if (m_lastDialogGeometry.isValid() && 
                m_lastDialogGeometry.contains(globalPos)) {
                m_lastDialogGeometry = QRect();  // 清除记录，只跳过一次
                break;
            }
            
            QWidget* target = QApplication::widgetAt(globalPos);
            if (!target) break;
            if (ControlInspector::shouldIgnore(target) &&
                !ControlInspector::isAreaClick(target)) break;
            m_processor->processMouseClick(target, globalPos);
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
            m_processor->processTouchTap(target, globalPos);
            break;
        }
        case QEvent::KeyPress: {
            auto* ke = static_cast<QKeyEvent*>(event);
            // 仅带修饰键的按键视为快捷键
            if (ke->modifiers() == Qt::NoModifier) break;
            m_processor->processShortcut(qobject_cast<QWidget*>(watched), ke);
            break;
        }
        default:
            break;
    }
    return false;  // 不拦截事件
}

} // namespace behavior
} // namespace ui_shared
