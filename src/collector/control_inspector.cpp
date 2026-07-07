#include "control_inspector.h"
#include "core/config.h"
#include <QWidget>
#include <QLabel>
#include <QAbstractButton>
#include <QLineEdit>
#include <QWindow>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QDialog>

namespace ui_shared {
namespace behavior {

const QStringList& ControlInspector::defaultIgnoreClasses() {
    static const QStringList list = {
        "QLabel", "QFrame", "QWidget", "QSpacerItem",
        "QScrollBar", "QToolTip", "QStatusBar", "QSizeGrip"
    };
    return list;
}

const QSet<QString>& ControlInspector::interactiveClasses() {
    static const QSet<QString> set = {
        "QPushButton", "QToolButton", "QCheckBox", "QRadioButton",
        "QComboBox", "QLineEdit", "QSpinBox", "QDoubleSpinBox",
        "QSlider", "QTabBar", "QListWidget", "QTableWidget",
        "QTreeWidget", "QMenuBar", "QMenu", "QAbstractButton"
    };
    return set;
}

QString ControlInspector::className(QObject* obj) {
    if (!obj) return {};
    return QString::fromLatin1(obj->metaObject()->className());
}

QString ControlInspector::objectName(QObject* obj) {
    return obj ? obj->objectName() : QString();
}

QString ControlInspector::displayText(QObject* obj) {
    if (!obj) return {};
    // 优先取标准文本属性
    QVariant v = obj->property("text");
    if (v.isValid()) return v.toString();
    v = obj->property("windowTitle");
    if (v.isValid()) return v.toString();
    v = obj->property("title");
    if (v.isValid()) return v.toString();
    return {};
}

QString ControlInspector::path(QObject* obj) {
    if (!obj) return {};
    QStringList parts;
    QObject* cur = obj;
    while (cur) {
        QString n = cur->objectName();
        if (n.isEmpty()) n = QString::fromLatin1(cur->metaObject()->className());
        parts.prepend(n);
        cur = cur->parent();
    }
    return parts.join('/');
}

QWidget* ControlInspector::window(QObject* obj) {
    auto* w = qobject_cast<QWidget*>(obj);
    return w ? w->window() : nullptr;
}

QString ControlInspector::windowClassName(QObject* obj) {
    QWidget* w = window(obj);
    return w ? QString::fromLatin1(w->metaObject()->className()) : QString();
}

QString ControlInspector::windowTitle(QObject* obj) {
    QWidget* w = window(obj);
    return w ? w->windowTitle() : QString();
}

QString ControlInspector::windowPath(QObject* obj) {
    QWidget* w = window(obj);
    return w ? path(w) : QString();
}

bool ControlInspector::isMainWindow(QObject* obj) {
    auto* w = qobject_cast<QWidget*>(obj);
    if (!w) return false;
    QWidget* win = w->window();
    if (!win || !win->isTopLevel()) return false;
    if (win->parentWidget()) return false;
    if (qobject_cast<QDialog*>(win)) return false;
    return true;
}

int ControlInspector::heatRegion(const QPoint& globalPos, QWidget* window) {
    if (!window) return -1;
    QPoint local = window->mapFromGlobal(globalPos);
    QRect rect = window->rect();
    int width = rect.width();
    int height = rect.height();
    if (width <= 0 || height <= 0) return -1;

    double relX = qBound(0.0, static_cast<double>(local.x()) / width, 1.0);
    double relY = qBound(0.0, static_cast<double>(local.y()) / height, 1.0);
    int col = qBound(0, static_cast<int>(relX * 10), 9);
    int row = qBound(0, static_cast<int>(relY * 10), 9);
    return row * 10 + col;
}

bool ControlInspector::isAreaClick(QWidget* target) {
    if (!target) return true;
    QString cls = className(target);
    if (interactiveClasses().contains(cls)) return false;
    if (shouldIgnore(target)) return true;
    if (!target->objectName().isEmpty()) return false;
    return true;
}

bool ControlInspector::shouldIgnore(QObject* obj) {
    if (!obj) return true;
    QString cls = className(obj);
    if (Config::instance().shouldIgnore(cls)) return true;
    if (defaultIgnoreClasses().contains(cls)) return true;
    auto* w = qobject_cast<QWidget*>(obj);
    if (w) {
        if (!w->isVisible() || !w->isEnabled()) return true;
    }
    return false;
}

} // namespace behavior
} // namespace ui_shared
