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
    Mouse,
    Touch,
    Keyboard
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
    }
    return "mouse";
}

inline InputMethod stringToInputMethod(const QString& s) {
    if (s == "touch")    return InputMethod::Touch;
    if (s == "keyboard") return InputMethod::Keyboard;
    return InputMethod::Mouse;
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
