#pragma once

#include <QObject>
#include <QRect>
#include <memory>

namespace ui_shared {
namespace behavior {

class EventProcessor;
class DialogTracker;

// 全局事件过滤器，安装到 QApplication
class EventFilter : public QObject {
    Q_OBJECT
public:
    explicit EventFilter(EventProcessor* processor, QObject* parent = nullptr);
    ~EventFilter() override;

    void install();
    void uninstall();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    EventProcessor* processor_;  // 非所有
    QRect lastDialogGeometry_;   // 记录最后关闭的对话框几何区域
};

} // namespace behavior
} // namespace ui_shared
