#include "time_tab.h"
#include "chart_widgets.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QPen>
#include <QBrush>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QVector>
#include <QStringList>
#include <QHash>
#include <QDate>
#include <algorithm>

#include <qwt_plot_barchart.h>
#include <qwt_plot_curve.h>

using namespace ui_shared::behavior;

TimeTab::TimeTab(QWidget* parent) : QWidget(parent) {
    chart_ = new QwtPlot(this);
    stylePlot(chart_);
    chart_->setAxisTitle(QwtPlot::yLeft, "Count");
    chart_->setAxisTitle(QwtPlot::xBottom, "Date");

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(chart_);
}

void TimeTab::updateData(const QDateTime& start, const QDateTime& end) {
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
    QVector<double> timeData;
    QStringList timeLabels;

    if (useAgg) {
        q.prepare("SELECT substr(time_bucket,1,10) as dt, SUM(count) as cnt FROM agg_time_distribution "
                  "WHERE time_bucket >= ? AND time_bucket <= ? GROUP BY dt ORDER BY dt");
        q.addBindValue(start.toString("yyyy-MM-dd")); q.addBindValue(end.toString("yyyy-MM-dd")); q.exec();
        while (q.next()) { timeLabels << q.value(0).toString(); timeData << q.value(1).toDouble(); }
    } else {
        q.prepare("SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) as dt, COUNT(*) as cnt "
                  "FROM operations WHERE time >= ? AND time < ? GROUP BY dt ORDER BY dt");
        q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
        while (q.next()) { timeLabels << q.value(0).toString(); timeData << q.value(1).toDouble(); }
    }

    // Fill missing days with zero
    {
        QHash<QString, double> dayMap;
        for (int i = 0; i < timeLabels.size(); ++i) dayMap[timeLabels[i]] = timeData[i];
        timeLabels.clear(); timeData.clear();
        for (QDate d = start.date(); d <= end.date(); d = d.addDays(1)) {
            QString key = d.toString("yyyy-MM-dd");
            timeLabels << key; timeData << dayMap.value(key, 0.0);
        }
    }

    if (!timeData.isEmpty()) {
        int n = timeData.size();
        QVector<double> x(n);
        for (int i = 0; i < n; ++i) x[i] = i;

        if (n <= 7) {
            // bar chart + curve for small ranges
            auto* chart = new QwtPlotBarChart("Time Distribution");
            styleBarChart(chart, QColor("#10B981"));
            chart->setSamples(timeData);
            chart->attach(chart_);
            if (n == 1) {
                chart_->setAxisScale(QwtPlot::xBottom, -0.5, 0.5, 1);
            } else {
                chart_->setAxisScale(QwtPlot::xBottom, 0, n - 1, 1);
            }
        }

        // always draw the curve
        auto* curve = new QwtPlotCurve("Time Distribution");
        curve->setSamples(x.data(), timeData.data(), n);
        curve->setPen(QPen(QColor("#2563EB"), 2));
        curve->setBrush(QBrush(QColor(37, 99, 235, 40)));
        curve->attach(chart_);

        int step = (n <= 30) ? 5 : (n <= 90) ? 10 : 30;
        chart_->setAxisScale(QwtPlot::xBottom, 0, n - 1, step);
        double maxVal = *std::max_element(timeData.begin(), timeData.end());
        chart_->setAxisScale(QwtPlot::yLeft, 0, qMax(maxVal * 1.2, 1.0));
        chart_->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(timeLabels));
        chart_->setAxisTitle(QwtPlot::xBottom,
            QString("Date (%1 to %2)").arg(start.toString("yyyy-MM-dd")).arg(end.toString("yyyy-MM-dd")));
        chart_->replot();
    } else {
        chart_->setAxisTitle(QwtPlot::xBottom, "Date (no data)");
        chart_->replot();
    }
}
