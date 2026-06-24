#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPoint>
#include <QSet>

class QWidget;

namespace ui_shared {
namespace behavior {

class ControlInspector {
public:
    static QString className(QObject* obj);
    static QString objectName(QObject* obj);
    static QString displayText(QObject* obj);
    static QString path(QObject* obj);

    static QWidget* window(QObject* obj);
    static QString windowClassName(QObject* obj);
    static QString windowTitle(QObject* obj);
    static QString windowPath(QObject* obj);
    static bool isMainWindow(QObject* obj);

    // 热区编号 (0-99)，globalPos 为全局坐标
    static int heatRegion(const QPoint& globalPos, QWidget* window);

    // 空白区域点击判定
    static bool isAreaClick(QWidget* target);

    // 可忽略控件判定
    static bool shouldIgnore(QObject* obj);

    // 交互控件白名单
    static const QSet<QString>& interactiveClasses();

private:
    static const QStringList& defaultIgnoreClasses();
};

} // namespace behavior
} // namespace ui_shared
