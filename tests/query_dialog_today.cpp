// query_dialog_today.cpp — 查今天的对话框原始数据
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>
#include <QDateTime>
#include <QDate>
#include "storage/database.h"
#include "core/config.h"

using namespace ui_shared::behavior;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Syble");
    QCoreApplication::setApplicationName("BehaviorDemo");

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/behavior.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);

    QTextStream out(stdout);
    out.setCodec("UTF-8");

    QDate today = QDate::currentDate();
    QDateTime start(today, QTime(0,0,0));
    QDateTime end(today, QTime(23,59,59));
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();

    // 1. 原始 dialog_open / dialog_close 事件
    out << "=== Today's dialog events (raw) ===\n";
    q.prepare("SELECT event_type, window_title, control_name, window_class, module, "
              "duration_ms, datetime(time/1000,'unixepoch','localtime') as dt, time "
              "FROM operations WHERE time >= ? AND time <= ? "
              "AND event_type IN ('dialog_open','dialog_close') ORDER BY time");
    q.addBindValue(startMs);
    q.addBindValue(endMs);
    q.exec();
    while (q.next()) {
        out << "  " << q.value(6).toString()
            << " | " << q.value(0).toString()
            << " | title=" << q.value(1).toString()
            << " | ctrl=" << q.value(2).toString()
            << " | wclass=" << q.value(3).toString()
            << " | module=" << q.value(4).toString()
            << " | dur=" << q.value(5).toString()
            << " | time=" << q.value(7).toLongLong()
            << "\n";
    }

    // 2. 模拟 dialog_analyzer 的逻辑
    out << "\n=== Dialog analyzer logic ===\n";
    q.prepare("SELECT event_type, window_title, control_name, window_class, module, duration_ms "
              "FROM operations WHERE time >= ? AND time <= ? "
              "AND event_type IN ('dialog_open','dialog_close') ORDER BY time");
    q.addBindValue(startMs);
    q.addBindValue(endMs);
    q.exec();

    struct DlgStats {
        int openCount = 0;
        int closeCount = 0;
        qint64 totalDur = 0;
        int minDur = 0, maxDur = 0;
    };
    QHash<QString, DlgStats> stats;

    while (q.next()) {
        QString et = q.value(0).toString();
        QString title = q.value(1).toString();
        QString ctrl = q.value(2).toString();
        QString wclass = q.value(3).toString();
        QString module = q.value(4).toString();
        qint64 dur = q.value(5).toLongLong();

        QString key;
        if (!title.isEmpty()) {
            key = title;
        } else if (!ctrl.isEmpty()) {
            key = ctrl;
        } else {
            key = wclass;
        }
        if (!module.isEmpty()) key = module + "/" + key;

        if (et == "dialog_open") {
            stats[key].openCount++;
        } else if (et == "dialog_close") {
            stats[key].closeCount++;
            stats[key].totalDur += dur;
            if (stats[key].minDur == 0 || dur < stats[key].minDur) stats[key].minDur = dur;
            if (dur > stats[key].maxDur) stats[key].maxDur = dur;
        }
    }

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        int avg = it->closeCount > 0 ? (int)(it->totalDur / it->closeCount) : 0;
        double avgSec = avg / 1000.0;
        double avgHours = avgSec / 3600.0;
        out << "  Key: " << it.key()
            << " | opens=" << it->openCount
            << " | closes=" << it->closeCount
            << " | avg_ms=" << avg
            << " | avg_sec=" << avgSec
            << " | avg_hours=" << avgHours
            << " | min=" << it->minDur
            << " | max=" << it->maxDur
            << "\n";
    }

    // 3. 看看 duration_ms 的原始值
    out << "\n=== All dialog_close with duration_ms today ===\n";
    q.prepare("SELECT window_title, control_name, window_class, module, duration_ms, "
              "datetime(time/1000,'unixepoch','localtime') as dt, time "
              "FROM operations WHERE time >= ? AND time <= ? "
              "AND event_type = 'dialog_close' ORDER BY time");
    q.addBindValue(startMs);
    q.addBindValue(endMs);
    q.exec();
    while (q.next()) {
        out << "  " << q.value(5).toString()
            << " | title=" << q.value(0).toString()
            << " | dur_ms=" << q.value(4).toLongLong()
            << " | dur_sec=" << (q.value(4).toLongLong() / 1000.0)
            << " | dur_hours=" << (q.value(4).toLongLong() / 3600000.0)
            << "\n";
    }

    Database::instance().close();
    return 0;
}
