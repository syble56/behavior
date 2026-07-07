#pragma once

#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <optional>

namespace ui_shared {
namespace behavior {

// 事件类型
enum class EventType {
    MouseClick,
    TouchTap,
    Shortcut,
    DialogOpen,
    DialogClose,
    AreaClick
};

// 输入方式
enum class InputMethod {
    Mouse,      // 鼠标点击/拖拽
    Touch,      // 触摸点击/滑动
    Keyboard,   // 键盘按键
    Scroll,     // 滚动（鼠标滑轮、触控板滚动）
    Knob,       // 旋钮
    Derived     // 衍生事件（dialog_open/close、选件开关等）
};

// 事件类型 <-> 字符串（数据库存储用）
inline const char* eventTypeToString(EventType t) {
    switch (t) {
        case EventType::MouseClick:  return "mouse_click";
        case EventType::TouchTap:    return "touch_tap";
        case EventType::Shortcut:    return "shortcut";
        case EventType::DialogOpen:  return "dialog_open";
        case EventType::DialogClose: return "dialog_close";
        case EventType::AreaClick:   return "area_click";
    }
    return "unknown";
}

inline EventType stringToEventType(const QString& s) {
    if (s == "mouse_click")  return EventType::MouseClick;
    if (s == "touch_tap")    return EventType::TouchTap;
    if (s == "shortcut")     return EventType::Shortcut;
    if (s == "dialog_open")  return EventType::DialogOpen;
    if (s == "dialog_close") return EventType::DialogClose;
    if (s == "area_click")   return EventType::AreaClick;
    return EventType::MouseClick;
}

inline const char* inputMethodToString(InputMethod m) {
    switch (m) {
        case InputMethod::Mouse:    return "mouse";
        case InputMethod::Touch:    return "touch";
        case InputMethod::Keyboard: return "keyboard";
        case InputMethod::Scroll:   return "scroll";
        case InputMethod::Knob:     return "knob";
        case InputMethod::Derived:  return "derived";
    }
    return "derived";
}

inline InputMethod stringToInputMethod(const QString& s) {
    if (s == "mouse")    return InputMethod::Mouse;
    if (s == "touch")    return InputMethod::Touch;
    if (s == "keyboard") return InputMethod::Keyboard;
    if (s == "scroll")   return InputMethod::Scroll;
    if (s == "knob")     return InputMethod::Knob;
    if (s == "derived")  return InputMethod::Derived;
    return InputMethod::Derived;
}

// 操作记录
struct Operation {
    qint64 id = 0;
    QString sessionId;
    qint64 timestamp = 0;
    EventType eventType = EventType::MouseClick;
    InputMethod inputMethod = InputMethod::Mouse;

    // 控件信息
    QString controlClass;
    QString controlName;
    QString controlText;
    QString controlPath;

    // 快捷键信息
    QString actionName;
    QString keySequence;

    // 窗口信息
    QString windowClass;
    QString windowTitle;
    QString windowPath;
    bool isMainWindow = false;

    // 业务上下文（选件/测量项等，由应用层设置）
    QString module;

    // 位置信息（全局屏幕坐标）
    int screenX = 0;
    int screenY = 0;
    int heatRegion = 0;  // 0-99，-1 表示无效

    // 耗时信息（对话框关闭时）
    std::optional<int> duration;
};

// 会话
struct Session {
    QString id;
    qint64 startTime = 0;
    qint64 endTime = 0;
    int durationSeconds = 0;
    int operationCount = 0;
};

// 查询过滤器
struct QueryFilter {
    QDateTime startTime;
    QDateTime endTime;
    QString sessionId;
    QString eventType;       // 空表示不限
    bool onlyMainWindow = false;
    int limit = 100000;
};

// 分析结果
struct AnalysisResult {
    bool success = false;
    QString error;
    QVariantMap data;
};

} // namespace behavior
} // namespace ui_shared
