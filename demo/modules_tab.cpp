#include "modules_tab.h"
#include "chart_widgets.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QVector>
#include <QStringList>
#include <algorithm>

#include <qwt_plot_barchart.h>

using namespace ui_shared::behavior;

ModulesTab::ModulesTab(QWidget* parent) : QWidget(parent) {
    chart_ = new QwtPlot(this);
    stylePlot(chart_);
    chart_->setAxisTitle(QwtPlot::yLeft, "Count");
    chart_->setAxisTitle(QwtPlot::xBottom, "Module");

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(chart_);
}

void ModulesTab::updateData(const QDateTime& start, const QDateTime& end) {
    auto& db = Database::instance();
    QSqlDatabase sqlDb = db.connection();
    if (!sqlDb.isOpen()) return;

    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    QString startBucket = start.toString("yyyy-MM-dd");
    QString endBucket = end.toString("yyyy-MM-dd");
    int rangeDays = start.date().daysTo(end.date()) + 1;

    bool useAgg = (rangeDays > 7);

    QSqlQuery q(sqlDb);
    chart_->detachItems();
    QVector<double> modData;
    QStringList modLabels;

    if (useAgg) {
        q.prepare("SELECT module_class, SUM(count) as cnt FROM agg_module_stats "
                  "WHERE time_bucket >= ? AND time_bucket <= ? GROUP BY module_class ORDER BY cnt DESC");
        q.addBindValue(startBucket); q.addBindValue(endBucket); q.exec();
        while (q.next()) { modLabels << q.value(0).toString(); modData << q.value(1).toDouble(); }
    } else {
        q.prepare("SELECT COALESCE(NULLIF(window_class,''), 'unknown') as module, COUNT(*) as cnt "
                  "FROM operations WHERE time >= ? AND time < ? GROUP BY module ORDER BY cnt DESC");
        q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
        while (q.next()) { modLabels << q.value(0).toString(); modData << q.value(1).toDouble(); }
    }

    if (!modData.isEmpty()) {
        auto* chart = new QwtPlotBarChart("Modules");
        styleBarChart(chart, QColor("#8B5CF6"));
        chart->setSamples(modData);
        chart->attach(chart_);
        if (modData.size() == 1) chart_->setAxisScale(QwtPlot::xBottom, -0.5, 0.5, 1);
        else chart_->setAxisScale(QwtPlot::xBottom, 0, modData.size() - 1, 1); {
            chart_->setAxisScale(QwtPlot::yLeft, 0, *std::max_element(modData.begin(), modData.end()) * 1.2);
        }
        chart_->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(modLabels));
        chart_->replot();
    }
}
