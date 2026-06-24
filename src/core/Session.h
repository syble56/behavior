#pragma once

#include <QObject>
#include <QString>
#include "Types.h"

namespace ui_shared {
namespace behavior {

// 会话管理：以应用启动为会话开始，关闭为结束
class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject* parent = nullptr);

    // 开始新会话，返回会话ID
    QString start();
    // 结束会话，填充 endTime/duration，返回会话记录
    Session end();
    // 当前会话
    const Session& current() const { return m_current; }
    bool isActive() const { return m_active; }

    // 生成 UUID（无花括号）
    static QString generateId();

private:
    Session m_current;
    bool m_active = false;
};

} // namespace behavior
} // namespace ui_shared
