// check_op_stats.cpp — 深挖 operation_stats 差异
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

    QFile log(QDir::currentPath() + "/check_op_log.txt");
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&log);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/behavior.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);

    // operation_stats 按 action_key 分组，看哪些 action_key 的 count 和原始数据对不上
    out << "=== action_key level comparison (day granularity) ===\n\n";

    q.exec("SELECT action_key, SUM(count) FROM agg_operation_stats WHERE granularity='day' GROUP BY action_key ORDER BY SUM(count) DESC");
    QList<QPair<QString,int>> aggCounts;
    while (q.next()) aggCounts.append({q.value(0).toString(), q.value(1).toInt()});

    // 原始数据按同样的 action_key 逻辑分组
    q.exec("SELECT COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), NULLIF(control_class,''), event_type) as ak, COUNT(*) "
           "FROM operations GROUP BY ak ORDER BY COUNT(*) DESC");
    QList<QPair<QString,int>> rawCounts;
    while (q.next()) rawCounts.append({q.value(0).toString(), q.value(1).toInt()});

    out << QString("Raw action_keys: %1, Agg action_keys: %2\n").arg(rawCounts.size()).arg(aggCounts.size());

    // 找差异
    QHash<QString, int> aggMap;
    for (const auto& p : aggCounts) aggMap[p.first] = p.second;

    int totalRaw = 0, totalAgg = 0, diffCount = 0;
    out << "\n=== Differences (raw > agg) ===\n";
    for (const auto& p : rawCounts) {
        totalRaw += p.second;
        int agg = aggMap.value(p.first, 0);
        totalAgg += agg;
        if (p.second != agg) {
            out << QString("  %1 : raw=%2, agg=%3, diff=%4\n").arg(p.first).arg(p.second).arg(agg).arg(p.second - agg);
            diffCount++;
        }
    }

    out << QString("\nTotal raw: %1, Total agg: %2, Diff: %3\n").arg(totalRaw).arg(totalAgg).arg(totalRaw - totalAgg);
    out << QString("Action keys with differences: %1\n").arg(diffCount);

    // 检查是否有 action_key 在 agg 中但不在 raw 中
    QHash<QString, int> rawMap;
    for (const auto& p : rawCounts) rawMap[p.first] = p.second;
    out << "\n=== Keys in agg but not in raw ===\n";
    for (const auto& p : aggCounts) {
        if (!rawMap.contains(p.first)) {
            out << QString("  %1 : agg=%2\n").arg(p.first).arg(p.second);
        }
    }

    // 看看 event_type 分布
    out << "\n=== Event type distribution (raw) ===\n";
    q.exec("SELECT event_type, COUNT(*) FROM operations GROUP BY event_type ORDER BY COUNT(*) DESC");
    while (q.next()) out << QString("  %1 : %2\n").arg(q.value(0).toString()).arg(q.value(1).toInt());

    // operation_stats 的 UNIQUE 约束
    out << "\n=== agg_operation_stats schema ===\n";
    q.exec("SELECT sql FROM sqlite_master WHERE name='agg_operation_stats'");
    if (q.next()) out << q.value(0).toString() << "\n";

    // 看看有没有同 action_key 但不同 action_type 的
    out << "\n=== action_key with multiple action_types ===\n";
    q.exec("SELECT action_key, GROUP_CONCAT(DISTINCT action_type), COUNT(DISTINCT action_type) "
           "FROM agg_operation_stats WHERE granularity='day' GROUP BY action_key HAVING COUNT(DISTINCT action_type) > 1");
    int multiType = 0;
    while (q.next()) {
        out << QString("  %1 : types=[%2], count=%3\n").arg(q.value(0).toString()).arg(q.value(1).toString()).arg(q.value(2).toInt());
        multiType++;
    }
    out << QString("Total multi-type action_keys: %1\n").arg(multiType);

    // 关键：INSERT OR REPLACE 的 UNIQUE 约束是否包含 action_type？
    // 如果 UNIQUE 是 (time_bucket, granularity, action_key) 而不含 action_type，
    // 那同一 action_key 的不同 event_type 会互相覆盖！
    out << "\n=== Check: same action_key, different event_type in raw ===\n";
    q.exec("SELECT COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), NULLIF(control_class,''), event_type) as ak, "
           "event_type, COUNT(*) FROM operations GROUP BY ak, event_type HAVING COUNT(DISTINCT event_type) > 1 ORDER BY ak LIMIT 20");
    bool found = false;
    while (q.next()) {
        found = true;
        out << QString("  action_key=%1, event_type=%2, count=%3\n").arg(q.value(0).toString()).arg(q.value(1).toString()).arg(q.value(2).toInt());
    }
    if (!found) {
        // Try another way
        q.exec("SELECT ak, COUNT(DISTINCT event_type) as types FROM ("
               "SELECT COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), NULLIF(control_class,''), event_type) as ak, event_type FROM operations) "
               "GROUP BY ak HAVING types > 1 ORDER BY types DESC LIMIT 20");
        while (q.next()) {
            out << QString("  action_key=%1, distinct_event_types=%2\n").arg(q.value(0).toString()).arg(q.value(1).toInt());
        }
    }

    Database::instance().close();
    out << "\nDone.\n";
    return 0;
}
