#include "aggregator.h"
#include "database.h"
#include <QSqlQuery>
#include <QSqlError>

namespace ui_shared {
namespace behavior {

Aggregator::Aggregator(QObject* parent) : QObject(parent) {}

QString Aggregator::formatTimeBucket(const QDateTime& t, Granularity g) {
    return (g == Granularity::Hour) ? t.toString("yyyy-MM-dd HH:00")
                                    : t.toString("yyyy-MM-dd");
}

void Aggregator::aggregateRange(const QDateTime& start, const QDateTime& end, Granularity g) {
    int processed = 0;
    aggregateOperations(start, end, g);
    aggregateModules(start, end, g);
    aggregateInputs(start, end, g);
    aggregateHeatmap(start, end, g);
    aggregateDialogs(start, end, g);
    if (g == Granularity::Hour)
        aggregateTimeDistribution(start, end);
    emit aggregationCompleted(processed);
}

void Aggregator::aggregateOperations(const QDateTime& start, const QDateTime& end, Granularity g) {
    QSqlDatabase db = Database::instance().connection();
    if (!db.isOpen()) return;
    QString fmt = (g == Granularity::Hour) ? "%Y-%m-%d %H:00" : "%Y-%m-%d";
    QString gran = (g == Granularity::Hour) ? "hour" : "day";
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    
    QString sql = QString(
        "INSERT OR REPLACE INTO agg_operation_stats (time_bucket,granularity,action_key,action_type,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), '%2', "
        "COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), NULLIF(control_class,''), event_type), "
        "event_type, COUNT(*) "
        "FROM operations WHERE time >= %3 AND time < %4 "
        "GROUP BY 1, 3, 4")
        .arg(fmt).arg(gran).arg(startMs).arg(endMs);
    
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning("[Behavior] agg operations failed: %s", qPrintable(q.lastError().text()));
    }
}

void Aggregator::aggregateModules(const QDateTime& start, const QDateTime& end, Granularity g) {
    QSqlDatabase db = Database::instance().connection();
    if (!db.isOpen()) return;
    QString fmt = (g == Granularity::Hour) ? "%Y-%m-%d %H:00" : "%Y-%m-%d";
    QString gran = (g == Granularity::Hour) ? "hour" : "day";
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    
    QString sql = QString(
        "INSERT OR REPLACE INTO agg_module_stats (time_bucket,granularity,module_class,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), '%2', "
        "COALESCE(NULLIF(window_class,''), 'unknown'), COUNT(*) "
        "FROM operations WHERE time >= %3 AND time < %4 "
        "GROUP BY 1, 3")
        .arg(fmt).arg(gran).arg(startMs).arg(endMs);
    
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning("[Behavior] agg modules failed: %s", qPrintable(q.lastError().text()));
    }
}

void Aggregator::aggregateInputs(const QDateTime& start, const QDateTime& end, Granularity g) {
    QSqlDatabase db = Database::instance().connection();
    if (!db.isOpen()) return;
    QString fmt = (g == Granularity::Hour) ? "%Y-%m-%d %H:00" : "%Y-%m-%d";
    QString gran = (g == Granularity::Hour) ? "hour" : "day";
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    
    QString sql = QString(
        "INSERT OR REPLACE INTO agg_input_stats (time_bucket,granularity,input_method,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), '%2', "
        "input_method, COUNT(*) "
        "FROM operations WHERE time >= %3 AND time < %4 "
        "GROUP BY 1, 3")
        .arg(fmt).arg(gran).arg(startMs).arg(endMs);
    
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning("[Behavior] agg inputs failed: %s", qPrintable(q.lastError().text()));
    }
}

void Aggregator::aggregateHeatmap(const QDateTime& start, const QDateTime& end, Granularity g) {
    QSqlDatabase db = Database::instance().connection();
    if (!db.isOpen()) return;
    QString fmt = (g == Granularity::Hour) ? "%Y-%m-%d %H:00" : "%Y-%m-%d";
    QString gran = (g == Granularity::Hour) ? "hour" : "day";
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    
    QString sql = QString(
        "INSERT OR REPLACE INTO agg_heatmap_stats (time_bucket,granularity,heat_region,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), '%2', "
        "heat_region, COUNT(*) "
        "FROM operations WHERE time >= %3 AND time < %4 "
        "AND is_main_window = 1 AND event_type IN ('mouse_click','touch_tap','area_click') "
        "GROUP BY 1, 3")
        .arg(fmt).arg(gran).arg(startMs).arg(endMs);
    
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning("[Behavior] agg heatmap failed: %s", qPrintable(q.lastError().text()));
    }
}

void Aggregator::aggregateDialogs(const QDateTime& start, const QDateTime& end, Granularity g) {
    QSqlDatabase db = Database::instance().connection();
    if (!db.isOpen()) return;
    QString fmt = (g == Granularity::Hour) ? "%Y-%m-%d %H:00" : "%Y-%m-%d";
    QString gran = (g == Granularity::Hour) ? "hour" : "day";
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    
    // 标识优先级与 dialog_analyzer 一致：windowTitle > controlName > windowClass
    // open_count 统计 dialog_open，时长统计 dialog_close
    QString sql = QString(
        "INSERT OR REPLACE INTO agg_dialog_stats (time_bucket,granularity,dialog_class,open_count,total_duration,avg_duration) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), '%2', "
        "COALESCE(NULLIF(window_title,''), NULLIF(control_name,''), window_class), "
        "SUM(CASE WHEN event_type='dialog_open' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,0) ELSE 0 END), "
        "AVG(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,0) END) "
        "FROM operations WHERE time >= %3 AND time < %4 "
        "AND event_type IN ('dialog_open','dialog_close') "
        "GROUP BY 1, 3")
        .arg(fmt).arg(gran).arg(startMs).arg(endMs);
    
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning("[Behavior] agg dialogs failed: %s", qPrintable(q.lastError().text()));
    }
}

void Aggregator::aggregateTimeDistribution(const QDateTime& start, const QDateTime& end) {
    QSqlDatabase db = Database::instance().connection();
    if (!db.isOpen()) return;
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    
    QString sql = QString(
        "INSERT OR REPLACE INTO agg_time_distribution (date,hour,count) "
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')), "
        "CAST(strftime('%H', datetime(time/1000,'unixepoch','localtime')) AS INTEGER), COUNT(*) "
        "FROM operations WHERE time >= %1 AND time < %2 GROUP BY 1, 2")
        .arg(startMs).arg(endMs);
    
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning("[Behavior] agg time_dist failed: %s", qPrintable(q.lastError().text()));
    }
}

} // namespace behavior
} // namespace ui_shared
