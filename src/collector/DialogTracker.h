#pragma once

#include <QObject>
#include <QHash>
#include <QString>

namespace ui_shared {
namespace behavior {

class DialogTracker {
public:
    DialogTracker();

    void onDialogOpen(QObject* dialog);
    int onDialogClose(QObject* dialog);  // 返回保留时长(ms)，0表示未记录打开

    static QString dialogId(QObject* dialog);

private:
    QHash<QString, qint64> m_openTimes;
};

} // namespace behavior
} // namespace ui_shared
