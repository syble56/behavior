#include "heatmap_tab.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QMap>

using namespace ui_shared::behavior;

HeatmapTab::HeatmapTab(QWidget* parent) : QWidget(parent) {
    widget_ = new HeatmapWidget(this);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(widget_);
}

void HeatmapTab::updateData(const QDateTime& start, const QDateTime& end) {
    auto& db = Database::instance();
    QSqlDatabase sqlDb = db.connection();
    if (!sqlDb.isOpen()) return;

    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    QString startBucket = start.toString("yyyy-MM-dd");
    QString endBucket = end.toString("yyyy-MM-dd");
    int rangeDays = start.date().daysTo(end.date()) + 1;

    bool useAgg = false;
    if (rangeDays > 90) {
        QSqlQuery q0(sqlDb);
        q0.exec("SELECT MIN(time_bucket), MAX(time_bucket) FROM agg_operation_stats WHERE length(time_bucket) = 10");
        QString aggMinDay, aggMaxDay;
        if (q0.next()) { aggMinDay = q0.value(0).toString(); aggMaxDay = q0.value(1).toString(); }
        if (!aggMinDay.isEmpty() && startBucket >= aggMinDay && endBucket <= aggMaxDay) useAgg = true;
    }

    QSqlQuery q(sqlDb);
    QMap<int, int> heatData;
    int maxHeat = 0;

    if (useAgg) {
        q.prepare("SELECT heat_region, SUM(count) as cnt FROM agg_heatmap_stats "
                  "WHERE time_bucket >= ? AND time_bucket <= ? GROUP BY heat_region");
        q.addBindValue(startBucket); q.addBindValue(endBucket); q.exec();
        while (q.next()) { int r = q.value(0).toInt(); int c = q.value(1).toInt(); heatData[r] = c; maxHeat = qMax(maxHeat, c); }
    } else {
        q.prepare("SELECT heat_region, COUNT(*) as cnt FROM operations WHERE time >= ? AND time < ? "
                  "AND is_main_window = 1 AND event_type IN ('mouse_click','touch_tap','area_click') GROUP BY heat_region");
        q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
        while (q.next()) { int r = q.value(0).toInt(); int c = q.value(1).toInt(); heatData[r] = c; maxHeat = qMax(maxHeat, c); }
    }
    widget_->setData(heatData, maxHeat);
}
