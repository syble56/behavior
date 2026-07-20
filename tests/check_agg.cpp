// check_agg.cpp — 验证聚合表与原始数据一致性
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

#include "storage/database.h"
#include "core/config.h"

using namespace ui_shared::behavior;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Syble");
    QCoreApplication::setApplicationName("BehaviorDemo");

    QFile log(QDir::currentPath() + "/check_log.txt");
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&log);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/behavior.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);

    out << "=== 1. operations total ===\n";
    q.exec("SELECT COUNT(*) FROM operations");
    if (q.next()) out << "  operations: " << q.value(0).toLongLong() << "\n";

    out << "\n=== 2. per-day from operations (raw) ===\n";
    q.exec("SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) as day, COUNT(*) "
           "FROM operations GROUP BY day ORDER BY day");
    qint64 rawTotal = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toLongLong() << "\n";
        rawTotal += q.value(1).toLongLong();
    }
    out << "  RAW TOTAL: " << rawTotal << "\n";

    out << "\n=== 3. agg_input_stats (day) ===\n";
    q.exec("SELECT time_bucket, SUM(count) FROM agg_input_stats WHERE granularity='day' GROUP BY time_bucket ORDER BY time_bucket");
    qint64 inputTotal = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toLongLong() << "\n";
        inputTotal += q.value(1).toLongLong();
    }
    out << "  INPUT TOTAL: " << inputTotal << "\n";

    out << "\n=== 4. agg_input_stats (hour) ===\n";
    q.exec("SELECT time_bucket, SUM(count) FROM agg_input_stats WHERE granularity='hour' GROUP BY time_bucket ORDER BY time_bucket");
    qint64 hourTotal = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toLongLong() << "\n";
        hourTotal += q.value(1).toLongLong();
    }
    out << "  HOUR TOTAL: " << hourTotal << "\n";

    out << "\n=== 5. agg_operation_stats (day) ===\n";
    q.exec("SELECT time_bucket, SUM(count) FROM agg_operation_stats WHERE granularity='day' GROUP BY time_bucket ORDER BY time_bucket");
    qint64 opTotal = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toLongLong() << "\n";
        opTotal += q.value(1).toLongLong();
    }
    out << "  OP TOTAL: " << opTotal << "\n";

    out << "\n=== 6. agg_module_stats (day) ===\n";
    q.exec("SELECT time_bucket, SUM(count) FROM agg_module_stats WHERE granularity='day' GROUP BY time_bucket ORDER BY time_bucket");
    qint64 modTotal = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toLongLong() << "\n";
        modTotal += q.value(1).toLongLong();
    }
    out << "  MODULE TOTAL: " << modTotal << "\n";

    out << "\n=== 7. agg_time_distribution ===\n";
    q.exec("SELECT substr(time_bucket,1,10) as dt, SUM(count) FROM agg_time_distribution GROUP BY dt ORDER BY dt");
    qint64 tdTotal = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toLongLong() << "\n";
        tdTotal += q.value(1).toLongLong();
    }
    out << "  TIME_DIST TOTAL: " << tdTotal << "\n";

    out << "\n=== 8. agg_heatmap_stats (day) ===\n";
    q.exec("SELECT time_bucket, SUM(count) FROM agg_heatmap_stats WHERE granularity='day' GROUP BY time_bucket ORDER BY time_bucket");
    qint64 hmTotal = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toLongLong() << "\n";
        hmTotal += q.value(1).toLongLong();
    }
    out << "  HEATMAP TOTAL: " << hmTotal << "\n";

    out << "\n=== 9. agg_dialog_stats (day) ===\n";
    q.exec("SELECT time_bucket, SUM(open_count) FROM agg_dialog_stats WHERE granularity='day' GROUP BY time_bucket ORDER BY time_bucket");
    qint64 dlgTotal = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toLongLong() << "\n";
        dlgTotal += q.value(1).toLongLong();
    }
    out << "  DIALOG TOTAL: " << dlgTotal << "\n";

    out << "\n=== SUMMARY ===\n";
    out << "  raw operations:     " << rawTotal << "\n";
    out << "  input_stats(day):   " << inputTotal << "\n";
    out << "  input_stats(hour):  " << hourTotal << "\n";
    out << "  operation_stats:    " << opTotal << "\n";
    out << "  module_stats:       " << modTotal << "\n";
    out << "  time_distribution:  " << tdTotal << "\n";
    out << "  heatmap_stats:      " << hmTotal << "\n";
    out << "  dialog_stats:       " << dlgTotal << "\n";

    // 差异检查
    out << "\n=== DIFF CHECK ===\n";
    if (rawTotal == inputTotal) {
        out << "  raw == input(day):  OK\n";
    } else {
        out << "  raw != input(day):  MISMATCH! diff=" << (rawTotal - inputTotal) << "\n";
    }
    if (rawTotal == hourTotal) {
        out << "  raw == input(hour): OK\n";
    } else {
        out << "  raw != input(hour): MISMATCH! diff=" << (rawTotal - hourTotal) << "\n";
    }
    if (rawTotal == opTotal) {
        out << "  raw == op_stats:    OK\n";
    } else {
        out << "  raw != op_stats:    MISMATCH! diff=" << (rawTotal - opTotal) << "\n";
    }
    if (rawTotal == modTotal) {
        out << "  raw == module:      OK\n";
    } else {
        out << "  raw != module:      MISMATCH! diff=" << (rawTotal - modTotal) << "\n";
    }
    if (rawTotal == tdTotal) {
        out << "  raw == time_dist:   OK\n";
    } else {
        out << "  raw != time_dist:   MISMATCH! diff=" << (rawTotal - tdTotal) << "\n";
    }

    Database::instance().close();
    out << "\nDone.\n";
    return 0;
}
