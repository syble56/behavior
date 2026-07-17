#include "dialog_tab.h"
#include "chart_widgets.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QVector>
#include <QStringList>
#include <algorithm>

#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>
#include <qwt_text.h>

using namespace ui_shared::behavior;

DialogTab::DialogTab(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    table_ = new QTableWidget(this);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels(
        {"Dialog", "Opens", "Avg Duration", "Median", "Min", "Max", "Instant Close %"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table_->setAlternatingRowColors(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lay->addWidget(table_, 1);

    scatter_ = new QwtPlot(this);
    stylePlot(scatter_);
    scatter_->setAxisTitle(QwtPlot::xBottom, "Opens");
    scatter_->setAxisTitle(QwtPlot::yLeft, "Avg Duration (s)");
    scatter_->setMinimumSize(500, 300);
    lay->addWidget(scatter_, 1);
}

void DialogTab::updateData(const QDateTime& start, const QDateTime& end) {
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

    struct DialogStat {
        QString cls;
        int openCount;
        double avgDurationMs;
    };

    QVector<DialogStat> dialogs;

    if (useAgg) {
        q.prepare("SELECT dialog_class, SUM(open_count) as opens, "
                  "CASE WHEN SUM(open_count) > 0 THEN SUM(total_duration)*1.0/SUM(open_count) ELSE 0 END as avg_dur "
                  "FROM agg_dialog_stats WHERE time_bucket >= ? AND time_bucket <= ? "
                  "GROUP BY dialog_class ORDER BY opens DESC");
        q.addBindValue(startBucket); q.addBindValue(endBucket); q.exec();
        while (q.next()) {
            DialogStat s;
            s.cls = q.value(0).toString();
            s.openCount = q.value(1).toInt();
            s.avgDurationMs = q.value(2).toDouble();
            dialogs.append(s);
        }
    } else {
        // Query raw operations for detailed stats
        q.prepare(
            "SELECT "
            "  CASE WHEN module IS NOT NULL AND module != '' THEN module || '/' ELSE '' END || "
            "  COALESCE(NULLIF(window_title,''), NULLIF(control_name,''), window_class) as dlg, "
            "  SUM(CASE WHEN event_type='dialog_open' THEN 1 ELSE 0 END) as opens, "
            "  AVG(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,0) END) as avg_dur, "
            "  MIN(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,999999999) END) as min_dur, "
            "  MAX(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,0) END) as max_dur "
            "FROM operations WHERE time >= ? AND time < ? AND event_type IN ('dialog_open','dialog_close') "
            "GROUP BY dlg ORDER BY opens DESC");
        q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
        while (q.next()) {
            DialogStat s;
            s.cls = q.value(0).toString();
            s.openCount = q.value(1).toInt();
            s.avgDurationMs = q.value(2).toDouble();
            dialogs.append(s);
        }
    }

    auto fmtMs = [](int ms) -> QString {
        if (ms <= 0) return "-";
        if (ms < 1000) return QString("%1ms").arg(ms);
        double sec = ms / 1000.0;
        if (sec < 60) return QString("%1s").arg(sec, 0, 'f', 1);
        int m = static_cast<int>(sec / 60);
        double s = sec - m * 60;
        if (m < 60) return QString("%1m%2s").arg(m).arg(s, 0, 'f', 0);
        int h = m / 60; int rm = m % 60;
        return QString("%1h%2m").arg(h).arg(rm);
    };

    table_->setRowCount(dialogs.size());
    for (int i = 0; i < dialogs.size(); ++i) {
        const auto& d = dialogs[i];
        table_->setItem(i, 0, new QTableWidgetItem(d.cls));
        table_->setItem(i, 1, new QTableWidgetItem(QString::number(d.openCount)));
        table_->setItem(i, 2, new QTableWidgetItem(fmtMs((int)d.avgDurationMs)));
        // Median/Min/Max/InstantClose not available from aggregation
        table_->setItem(i, 3, new QTableWidgetItem("-"));
        table_->setItem(i, 4, new QTableWidgetItem("-"));
        table_->setItem(i, 5, new QTableWidgetItem("-"));
        table_->setItem(i, 6, new QTableWidgetItem("-"));
    }

    // Scatter plot
    scatter_->detachItems();
    QVector<double> sx, sy;
    QStringList sLabels;
    double maxX = 0, maxY = 0;
    for (const auto& d : dialogs) {
        if (d.openCount <= 0) continue;
        double avgSec = d.avgDurationMs / 1000.0;
        sx << d.openCount; sy << avgSec;
        QString label = d.cls;
        if (label.length() > 15) label = label.left(12) + "...";
        sLabels << label;
        maxX = qMax(maxX, (double)d.openCount); maxY = qMax(maxY, avgSec);
    }

    if (!sx.isEmpty()) {
        auto* curve = new QwtPlotCurve("Dialog");
        curve->setStyle(QwtPlotCurve::Dots);
        curve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
            QBrush(QColor("#3B82F6")), QPen(QColor("#1D4ED8"), 2), QSize(14, 14)));
        curve->setSamples(sx.data(), sy.data(), sx.size());
        curve->attach(scatter_);

        for (int i = 0; i < sLabels.size(); ++i) {
            QwtText label(sLabels[i]);
            label.setFont(QFont("Microsoft YaHei", 8));
            label.setColor(QColor("#E2E8F0"));
            auto* marker = new QwtPlotMarker();
            marker->setValue(sx[i] + maxX * 0.01, sy[i]);
            marker->setLabel(label);
            marker->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            marker->attach(scatter_);
        }

        scatter_->setAxisScale(QwtPlot::xBottom, 0, maxX * 1.15, 0);
        scatter_->setAxisScale(QwtPlot::yLeft, 0, maxY * 1.15, 0);
        scatter_->replot();
    }
}
