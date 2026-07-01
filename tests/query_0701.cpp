// query_0701.cpp — 用 Qt 本地时间正确查询 7/01 数据
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

#include "storage/Database.h"
#include "core/Config.h"

using namespace ui_shared::behavior;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Syble");
    QCoreApplication::setApplicationName("BehaviorDemo");

    QFile log(QDir::currentPath() + "/query_log.txt");
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&log);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/behavior.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);

    // 用 Qt 本地时间转 ms，避免 strftime 的 UTC 问题
    QDateTime dayStart = QDateTime::fromString("2026-07-01 00:00:00", "yyyy-MM-dd HH:mm:ss");
    QDateTime dayEnd   = QDateTime::fromString("2026-07-02 00:00:00", "yyyy-MM-dd HH:mm:ss");
    qint64 startMs = dayStart.toMSecsSinceEpoch();
    qint64 endMs   = dayEnd.toMSecsSinceEpoch();

    out << "Query range (local): " << dayStart.toString() << " ~ " << dayEnd.toString() << "\n";
    out << "Query range (ms): " << startMs << " ~ " << endMs << "\n\n";

    out << "=== 7/01 raw operations by hour (local time) ===\n";
    q.exec(QStringLiteral("SELECT strftime('%H:00', datetime(time/1000,'unixepoch','localtime')) as hr, COUNT(*) "
           "FROM operations WHERE time >= %1 AND time < %2 GROUP BY hr ORDER BY hr").arg(startMs).arg(endMs));
    int rawTotal = 0;
    while (q.next()) { out << "  " << q.value(0).toString() << " : " << q.value(1).toInt() << "\n"; rawTotal += q.value(1).toInt(); }
    out << "  RAW TOTAL: " << rawTotal << "\n";

    out << "\n=== 7/01 agg_input_stats (day) ===\n";
    q.exec("SELECT time_bucket, input_method, count FROM agg_input_stats WHERE granularity='day' AND time_bucket='2026-07-01'");
    int dayTotal = 0;
    while (q.next()) { out << "  " << q.value(1).toString() << " : " << q.value(2).toInt() << "\n"; dayTotal += q.value(2).toInt(); }
    out << "  DAY TOTAL: " << dayTotal << "\n";

    out << "\n=== 7/01 agg_input_stats (hour) ===\n";
    q.exec("SELECT time_bucket, input_method, count FROM agg_input_stats WHERE granularity='hour' AND time_bucket LIKE '2026-07-01%' ORDER BY time_bucket");
    int hourTotal = 0;
    while (q.next()) { out << "  " << q.value(0).toString() << " " << q.value(1).toString() << " : " << q.value(2).toInt() << "\n"; hourTotal += q.value(2).toInt(); }
    out << "  HOUR TOTAL: " << hourTotal << "\n";

    out << "\n=== 7/01 agg_operation_stats (day) sum ===\n";
    q.exec("SELECT SUM(count) FROM agg_operation_stats WHERE granularity='day' AND time_bucket='2026-07-01'");
    if (q.next()) out << "  OP TOTAL: " << q.value(0).toInt() << "\n";

    out << "\n=== 7/01 agg_module_stats (day) sum ===\n";
    q.exec("SELECT SUM(count) FROM agg_module_stats WHERE granularity='day' AND time_bucket='2026-07-01'");
    if (q.next()) out << "  MODULE TOTAL: " << q.value(0).toInt() << "\n";

    out << "\n=== 7/01 agg_time_distribution ===\n";
    q.exec("SELECT hour, count FROM agg_time_distribution WHERE date='2026-07-01' ORDER BY hour");
    int tdTotal = 0;
    while (q.next()) { out << "  hr " << q.value(0).toInt() << " : " << q.value(1).toInt() << "\n"; tdTotal += q.value(1).toInt(); }
    out << "  TD TOTAL: " << tdTotal << "\n";

    out << "\n=== 7/01 time range in operations ===\n";
    q.exec(QStringLiteral("SELECT datetime(MIN(time)/1000,'unixepoch','localtime'), datetime(MAX(time)/1000,'unixepoch','localtime') FROM operations WHERE time >= %1 AND time < %2").arg(startMs).arg(endMs));
    if (q.next()) out << "  from " << q.value(0).toString() << " to " << q.value(1).toString() << "\n";

    // 也查一下 6/24 做对比
    out << "\n=== 6/24 for comparison ===\n";
    QDateTime d24s = QDateTime::fromString("2026-06-24 00:00:00", "yyyy-MM-dd HH:mm:ss");
    QDateTime d24e = QDateTime::fromString("2026-06-25 00:00:00", "yyyy-MM-dd HH:mm:ss");
    q.exec(QStringLiteral("SELECT COUNT(*) FROM operations WHERE time >= %1 AND time < %2").arg(d24s.toMSecsSinceEpoch()).arg(d24e.toMSecsSinceEpoch()));
    if (q.next()) out << "  6/24 raw: " << q.value(0).toInt() << "\n";
    q.exec("SELECT SUM(count) FROM agg_input_stats WHERE granularity='day' AND time_bucket='2026-06-24'");
    if (q.next()) out << "  6/24 agg_input(day): " << q.value(0).toInt() << "\n";
    q.exec("SELECT SUM(count) FROM agg_input_stats WHERE granularity='hour' AND time_bucket LIKE '2026-06-24%'");
    if (q.next()) out << "  6/24 agg_input(hour): " << q.value(0).toInt() << "\n";

    // 查所有天的 raw vs agg 对比
    out << "\n=== All days raw vs agg_input(day) ===\n";
    for (int d = 24; d <= 30; d++) {
        QString ds = QStringLiteral("2026-06-%1 00:00:00").arg(d, 2, 10, QChar('0'));
        QString de = QStringLiteral("2026-06-%1 00:00:00").arg(d+1, 2, 10, QChar('0'));
        QString bucket = QStringLiteral("2026-06-%1").arg(d, 2, 10, QChar('0'));
        qint64 s = QDateTime::fromString(ds, "yyyy-MM-dd HH:mm:ss").toMSecsSinceEpoch();
        qint64 e = QDateTime::fromString(de, "yyyy-MM-dd HH:mm:ss").toMSecsSinceEpoch();
        q.exec(QStringLiteral("SELECT COUNT(*) FROM operations WHERE time >= %1 AND time < %2").arg(s).arg(e));
        int raw = (q.next()) ? q.value(0).toInt() : 0;
        q.exec(QStringLiteral("SELECT SUM(count) FROM agg_input_stats WHERE granularity='day' AND time_bucket='%1'").arg(bucket));
        int agg = (q.next()) ? q.value(0).toInt() : 0;
        out << QStringLiteral("  %1 : raw=%2, agg=%3, %4\n").arg(bucket).arg(raw).arg(agg).arg(raw==agg ? "OK" : "MISMATCH");
    }
    // 7/01
    q.exec(QStringLiteral("SELECT COUNT(*) FROM operations WHERE time >= %1 AND time < %2").arg(startMs).arg(endMs));
    int raw701 = (q.next()) ? q.value(0).toInt() : 0;
    q.exec("SELECT SUM(count) FROM agg_input_stats WHERE granularity='day' AND time_bucket='2026-07-01'");
    int agg701 = (q.next()) ? q.value(0).toInt() : 0;
    out << QStringLiteral("  2026-07-01 : raw=%1, agg=%2, %3\n").arg(raw701).arg(agg701).arg(raw701==agg701 ? "OK" : "MISMATCH");

    Database::instance().close();
    out << "\nDone.\n";
    return 0;
}
