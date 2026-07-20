// check_dialog.cpp — 模拟弹窗查询逻辑，看时间分布会显示什么
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
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

    QFile log(QDir::currentPath() + "/dialog_log.txt");
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&log);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/behavior.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);

    // 1. 弹窗的判断逻辑
    q.exec("SELECT COUNT(*) FROM agg_operation_stats");
    int aggCount = 0;
    if (q.next()) aggCount = q.value(0).toInt();
    bool useAgg = (aggCount > 0);
    out << "agg_operation_stats count: " << aggCount << "\n";
    out << "useAgg: " << (useAgg ? "true" : "false") << "\n\n";

    // 2. 弹窗默认日期范围
    QDate startDate = QDate::currentDate().addDays(-7);
    QDate endDate = QDate::currentDate();
    QDateTime start(startDate, QTime(0, 0, 0));
    QDateTime end(endDate, QTime(23, 59, 59));
    QString startBucket = start.toString("yyyy-MM-dd");
    QString endBucket = end.toString("yyyy-MM-dd");
    out << "Dialog date range: " << startBucket << " ~ " << endBucket << "\n\n";

    // 3. 时间分布：useAgg 路径
    out << "=== Time distribution (useAgg path) ===\n";
    q.prepare("SELECT substr(time_bucket,1,10) as dt, SUM(count) as cnt FROM agg_time_distribution WHERE time_bucket >= ? AND time_bucket <= ? GROUP BY dt ORDER BY dt");
    q.addBindValue(startBucket);
    q.addBindValue(endBucket);
    q.exec();
    int cnt = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toInt() << "\n";
        cnt++;
    }
    out << "  Total data points: " << cnt << "\n\n";

    // 4. 时间分布：直接查 operations
    out << "=== Time distribution (direct operations query) ===\n";
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    q.prepare("SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) as dt, COUNT(*) as cnt FROM operations WHERE time >= ? AND time < ? GROUP BY dt ORDER BY dt");
    q.addBindValue(startMs);
    q.addBindValue(endMs);
    q.exec();
    cnt = 0;
    while (q.next()) {
        out << "  " << q.value(0).toString() << " : " << q.value(1).toInt() << "\n";
        cnt++;
    }
    out << "  Total data points: " << cnt << "\n\n";

    // 5. agg_time_distribution 实际内容
    out << "=== agg_time_distribution actual content ===\n";
    q.exec("SELECT time_bucket, count FROM agg_time_distribution ORDER BY time_bucket");
    while (q.next()) out << "  " << q.value(0).toString() << " : " << q.value(1).toInt() << "\n";

    // 6. operations 表内容概况
    out << "\n=== operations summary ===\n";
    q.exec("SELECT COUNT(*) FROM operations");
    if (q.next()) out << "  total: " << q.value(0).toInt() << "\n";
    q.exec("SELECT datetime(MIN(time)/1000,'unixepoch','localtime'), datetime(MAX(time)/1000,'unixepoch','localtime') FROM operations");
    if (q.next()) out << "  range: " << q.value(0).toString() << " ~ " << q.value(1).toString() << "\n";

    // 7. 所有聚合表概况
    out << "\n=== All agg tables ===\n";
    for (const auto& t : QStringList{"agg_operation_stats","agg_module_stats","agg_input_stats","agg_heatmap_stats","agg_dialog_stats","agg_time_distribution"}) {
        q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(t));
        if (q.next()) out << "  " << t << ": " << q.value(0).toInt() << " rows\n";
    }

    // 8. agg_operation_stats 的 time_bucket 范围
    out << "\n=== agg_operation_stats time_bucket range ===\n";
    q.exec("SELECT MIN(time_bucket), MAX(time_bucket), COUNT(DISTINCT time_bucket) FROM agg_operation_stats");
    if (q.next()) out << "  min=" << q.value(0).toString() << " max=" << q.value(1).toString() << " distinct=" << q.value(2).toInt() << "\n";

    // 9. 模拟操作频率查询 (useAgg path)
    out << "\n=== Operation frequency (useAgg path) ===\n";
    q.prepare("SELECT action_key, SUM(count) as cnt FROM agg_operation_stats WHERE time_bucket >= ? AND time_bucket <= ? GROUP BY action_key ORDER BY cnt DESC LIMIT 15");
    q.addBindValue(startBucket);
    q.addBindValue(endBucket);
    q.exec();
    while (q.next()) out << "  " << q.value(0).toString() << " : " << q.value(1).toInt() << "\n";

    Database::instance().close();
    out << "\nDone.\n";
    return 0;
}
