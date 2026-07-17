#include "distance_tab.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QDate>
#include <QVector>
#include <algorithm>
#include <cmath>

#include <qwt_plot_curve.h>
#include <qwt_plot_picker.h>
#include <qwt_picker_machine.h>
#include <qwt_symbol.h>

using namespace ui_shared::behavior;

DistanceTab::DistanceTab(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(6);
    lay->setContentsMargins(0, 0, 0, 0);

    chart_ = new QwtPlot(this);
    stylePlot(chart_);
    chart_->setAxisTitle(QwtPlot::yLeft, "Avg Distance (px)");
    chart_->setAxisTitle(QwtPlot::xBottom, "Date");
    chart_->setMinimumHeight(220);
    lay->addWidget(chart_, 2);

    detailLabel_ = new QLabel(
        "Click a point in the chart above to view operation trajectory", this);
    detailLabel_->setStyleSheet(
        "color: #94A3B8; font-size: 12px; padding: 4px 8px; "
        "background-color: #3C3F44; border-radius: 4px;");
    lay->addWidget(detailLabel_);

    pathWidget_ = new PathWidget(this);
    lay->addWidget(pathWidget_, 3);

    // Click picker
    auto* picker = new QwtPlotPicker(chart_->canvas());
    picker->setStateMachine(new QwtPickerClickPointMachine());
    picker->setTrackerMode(QwtPicker::ActiveOnly);
    connect(picker, SIGNAL(selected(const QPointF&)),
            this, SLOT(onChartClicked(const QPointF&)));
}

void DistanceTab::updateData(const QDateTime& start, const QDateTime& end) {
    auto& db = Database::instance();
    QSqlDatabase sqlDb = db.connection();
    if (!sqlDb.isOpen()) return;

    // Query screen dimensions (max coords across all data)
    queryScreenSize();

    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();

    QSqlQuery q(sqlDb);
    
    // For large ranges, limit to sampled data to avoid loading 500k+ rows
    int rangeDaysDist = start.date().daysTo(end.date()) + 1;
    if (rangeDaysDist > 30) {
        // Use sampling: take every Nth record to keep memory manageable
        q.prepare(
            "SELECT screen_x, screen_y, "
            "strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) as dt "
            "FROM operations WHERE time >= ? AND time < ? "
            "AND event_type IN ('mouse_click','touch_tap','area_click') "
            "AND screen_x IS NOT NULL AND screen_y IS NOT NULL "
            "AND (screen_x != 0 OR screen_y != 0) "
            "AND (id % ? = 0) "  // sampling
            "ORDER BY time");
        q.addBindValue(startMs); q.addBindValue(endMs);
        q.addBindValue(qMax(1, rangeDaysDist / 30));  // ~30 points per day max
        q.exec();
    } else {
        q.prepare(
            "SELECT screen_x, screen_y, "
            "strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) as dt "
            "FROM operations WHERE time >= ? AND time < ? "
            "AND event_type IN ('mouse_click','touch_tap','area_click') "
            "AND screen_x IS NOT NULL AND screen_y IS NOT NULL "
            "AND (screen_x != 0 OR screen_y != 0) "
            "ORDER BY time");
        q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
    }

    // Group points by day
    QMap<QString, QList<QPointF>> dayPoints;
    while (q.next()) {
        int sx = q.value(0).toInt();
        int sy = q.value(1).toInt();
        QString dt = q.value(2).toString();
        dayPoints[dt].append(QPointF(sx, sy));
    }

    // Build per-day average distance + store trajectory data
    QVector<double> distData;
    QStringList distDates;
    double totalAvgDist = 0;
    int daysWithDist = 0;

    trajectoryData_.clear();
    for (QDate d = start.date(); d <= end.date(); d = d.addDays(1)) {
        QString key = d.toString("yyyy-MM-dd");
        QList<QPointF> pts = dayPoints.value(key);

        distDates << key;
        trajectoryData_[key] = pts;

        if (pts.size() < 2) {
            distData << 0.0;
            continue;
        }

        double dayTotal = 0;
        for (int i = 1; i < pts.size(); ++i) {
            double dx = pts[i].x() - pts[i - 1].x();
            double dy = pts[i].y() - pts[i - 1].y();
            dayTotal += std::sqrt(dx * dx + dy * dy);
        }
        double dayAvg = dayTotal / (pts.size() - 1);
        distData << dayAvg;
        totalAvgDist += dayAvg;
        daysWithDist++;
    }

    double overallAvg = daysWithDist > 0 ? totalAvgDist / daysWithDist : 0;
    emit avgDistanceChanged(qRound(overallAvg));

    // Plot distance over time
    chart_->detachItems();
    dates_ = distDates;

    if (!distData.isEmpty()) {
        int n = distData.size();
        QVector<double> x(n);
        for (int i = 0; i < n; ++i) x[i] = i;

        auto* curve = new QwtPlotCurve("Avg Distance");
        curve->setSamples(x.data(), distData.data(), n);
        curve->setPen(QPen(QColor("#F59E0B"), 2));
        curve->setBrush(QBrush(QColor(245, 158, 11, 40)));
        curve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
            QBrush(QColor("#F59E0B")), QPen(QColor("#FFFFFF"), 1.5), QSize(8, 8)));
        curve->attach(chart_);

        int step = (n <= 7) ? 1 : (n <= 30) ? 5 : (n <= 90) ? 10 : 30;
        chart_->setAxisScale(QwtPlot::xBottom, 0, n - 1, step);
        double maxVal = *std::max_element(distData.begin(), distData.end());
        chart_->setAxisScale(QwtPlot::yLeft, 0, qMax(maxVal * 1.2, 1.0));
        chart_->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(distDates));
        chart_->replot();
    }

    // Auto-select last day with data
    for (int i = distDates.size() - 1; i >= 0; --i) {
        if (!trajectoryData_[distDates[i]].isEmpty()) {
            selectDay(i);
            break;
        }
    }
}

void DistanceTab::onChartClicked(const QPointF& pos) {
    int idx = static_cast<int>(pos.x() + 0.5);
    selectDay(idx);
}

void DistanceTab::selectDay(int index) {
    if (index < 0 || index >= dates_.size()) return;

    QString date = dates_[index];
    QList<QPointF> trajectory = trajectoryData_.value(date);

    if (trajectory.isEmpty()) {
        detailLabel_->setText(
            QString("Selected: %1 | No trajectory data").arg(date));
        pathWidget_->clear();
        return;
    }

    // Compute stats for the selected day
    double totalDist = 0;
    double maxDist = 0;
    for (int i = 1; i < trajectory.size(); ++i) {
        double dx = trajectory[i].x() - trajectory[i - 1].x();
        double dy = trajectory[i].y() - trajectory[i - 1].y();
        double d = std::sqrt(dx * dx + dy * dy);
        totalDist += d;
        maxDist = qMax(maxDist, d);
    }
    double avgDist = trajectory.size() > 1 ? totalDist / (trajectory.size() - 1) : 0;

    detailLabel_->setText(QString(
        "Selected: %1 | Points: %2 | Avg: %3px | Max: %4px | Total: %5px")
        .arg(date).arg(trajectory.size())
        .arg(qRound(avgDist)).arg(qRound(maxDist)).arg(qRound(totalDist)));

    pathWidget_->setData(trajectory, screenW_, screenH_);
}

void DistanceTab::queryScreenSize() {
    auto* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect geo = screen->geometry();
        screenW_ = geo.width();
        screenH_ = geo.height();
    }
}
