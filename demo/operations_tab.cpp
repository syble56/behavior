#include "operations_tab.h"
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

OperationsTab::OperationsTab(QWidget* parent) : QWidget(parent) {
    chart_ = new QwtPlot(this);
    stylePlot(chart_);
    chart_->setAxisTitle(QwtPlot::yLeft, "Count");
    chart_->setAxisTitle(QwtPlot::xBottom, "Action");

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(chart_);
}

void OperationsTab::updateData(const QDateTime& start, const QDateTime& end) {
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
    QVector<double> opData;
    QStringList opLabels;

    if (useAgg) {
        q.prepare("SELECT action_key, SUM(count) as cnt FROM agg_operation_stats "
                  "WHERE time_bucket >= ? AND time_bucket <= ? GROUP BY action_key ORDER BY cnt DESC LIMIT 15");
        q.addBindValue(startBucket); q.addBindValue(endBucket); q.exec();
        while (q.next()) { opLabels << q.value(0).toString(); opData << q.value(1).toDouble(); }
        if (opData.isEmpty()) {
            q.prepare("SELECT COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), event_type) as action, COUNT(*) as cnt "
                      "FROM operations WHERE time >= ? AND time < ? GROUP BY action ORDER BY cnt DESC LIMIT 15");
            q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
            while (q.next()) { opLabels << q.value(0).toString(); opData << q.value(1).toDouble(); }
        }
    } else {
        q.prepare("SELECT COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), event_type) as action, COUNT(*) as cnt "
                  "FROM operations WHERE time >= ? AND time < ? GROUP BY action ORDER BY cnt DESC LIMIT 15");
        q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
        while (q.next()) { opLabels << q.value(0).toString(); opData << q.value(1).toDouble(); }
    }

    if (!opData.isEmpty()) {
        auto* chart = new QwtPlotBarChart("Operations");
        styleBarChart(chart, QColor("#3B82F6"));
        chart->setSamples(opData);
        chart->attach(chart_);
        chart_->setAxisScale(QwtPlot::xBottom, 0, opData.size() - 1, 1);
        chart_->setAxisScale(QwtPlot::yLeft, 0, *std::max_element(opData.begin(), opData.end()) * 1.2);
        chart_->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(opLabels));
        chart_->replot();
    }
}
