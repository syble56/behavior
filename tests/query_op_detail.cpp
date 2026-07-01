// query_op_detail.cpp — 分析 agg_operation_stats 的数据分布
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "storage/Database.h"
#include "core/Config.h"

using namespace ui_shared::behavior;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Syble");
    QCoreApplication::setApplicationName("BehaviorDemo");

    QFile log(QDir::currentPath() + "/op_detail_log.txt");
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&log);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/behavior.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);

    // 1. 总行数按粒度
    out << "=== Row count by granularity ===\n";
    q.exec("SELECT granularity, COUNT(*), SUM(count) FROM agg_operation_stats GROUP BY granularity");
    while (q.next()) out << "  " << q.value(0).toString() << ": rows=" << q.value(1).toInt() << ", sum_count=" << q.value(2).toLongLong() << "\n";

    // 2. 有多少 distinct action_key 和 action_type
    out << "\n=== Distinct values ===\n";
    q.exec("SELECT COUNT(DISTINCT action_key), COUNT(DISTINCT action_type) FROM agg_operation_stats");
    if (q.next()) out << "  distinct action_keys: " << q.value(0).toInt() << ", distinct action_types: " << q.value(1).toInt() << "\n";

    q.exec("SELECT DISTINCT action_type FROM agg_operation_stats ORDER BY action_type");
    out << "  action_types: ";
    while (q.next()) out << q.value(0).toString() << " ";
    out << "\n";

    // 3. count 分布
    out << "\n=== Count distribution ===\n";
    q.exec("SELECT CASE WHEN count=1 THEN '1' WHEN count<=5 THEN '2-5' WHEN count<=10 THEN '6-10' "
           "WHEN count<=20 THEN '11-20' WHEN count<=50 THEN '21-50' ELSE '50+' END as bucket, "
           "COUNT(*) as rows, SUM(count) as total_ops "
           "FROM agg_operation_stats GROUP BY bucket ORDER BY bucket");
    while (q.next()) out << "  count " << q.value(0).toString().leftJustified(6) << ": " << q.value(1).toInt() << " rows, " << q.value(2).toLongLong() << " ops\n";

    // 4. day 粒度：每天有多少行
    out << "\n=== Day granularity: rows per day ===\n";
    q.exec("SELECT time_bucket, COUNT(*) as rows, SUM(count) as ops FROM agg_operation_stats WHERE granularity='day' GROUP BY time_bucket ORDER BY time_bucket");
    while (q.next()) out << "  " << q.value(0).toString() << ": " << q.value(1).toInt() << " rows, " << q.value(2).toLongLong() << " ops\n";

    // 5. hour 粒度：每个桶有多少行
    out << "\n=== Hour granularity: rows per bucket (first 10) ===\n";
    q.exec("SELECT time_bucket, COUNT(*) as rows, SUM(count) as ops FROM agg_operation_stats WHERE granularity='hour' GROUP BY time_bucket ORDER BY time_bucket LIMIT 10");
    while (q.next()) out << "  " << q.value(0).toString() << ": " << q.value(1).toInt() << " rows, " << q.value(2).toLongLong() << " ops\n";

    // 6. 最大的 count 值（day 粒度）
    out << "\n=== Top 10 highest count (day granularity) ===\n";
    q.exec("SELECT time_bucket, action_key, action_type, count FROM agg_operation_stats WHERE granularity='day' ORDER BY count DESC LIMIT 10");
    while (q.next()) out << "  " << q.value(0).toString() << " | " << q.value(1).toString().leftJustified(16) << " | " << q.value(2).toString().leftJustified(12) << " | " << q.value(3).toInt() << "\n";

    // 7. 最大的 count 值（hour 粒度）
    out << "\n=== Top 10 highest count (hour granularity) ===\n";
    q.exec("SELECT time_bucket, action_key, action_type, count FROM agg_operation_stats WHERE granularity='hour' ORDER BY count DESC LIMIT 10");
    while (q.next()) out << "  " << q.value(0).toString() << " | " << q.value(1).toString().leftJustified(16) << " | " << q.value(2).toString().leftJustified(12) << " | " << q.value(3).toInt() << "\n";

    // 8. 示例：看一个 action_key 的所有行
    out << "\n=== Example: btnOk all rows (day) ===\n";
    q.exec("SELECT time_bucket, action_key, action_type, count FROM agg_operation_stats WHERE granularity='day' AND action_key='btnOk' ORDER BY time_bucket, action_type");
    while (q.next()) out << "  " << q.value(0).toString() << " | " << q.value(1).toString() << " | " << q.value(2).toString() << " | " << q.value(3).toInt() << "\n";

    // 9. 对比原始数据
    out << "\n=== Raw: btnOk by day and event_type ===\n";
    q.exec("SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) as day, event_type, COUNT(*) "
           "FROM operations WHERE COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), NULLIF(control_class,''), event_type)='btnOk' "
           "GROUP BY day, event_type ORDER BY day, event_type");
    while (q.next()) out << "  " << q.value(0).toString() << " | btnOk | " << q.value(1).toString() << " | " << q.value(2).toInt() << "\n";

    Database::instance().close();
    out << "\nDone.\n";
    return 0;
}
