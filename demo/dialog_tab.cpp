#include "dialog_tab.h"
#include "chart_widgets.h"
#include "core/behavior_analytics.h"
#include "analyzer/behavior_analyzer.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QVariantList>
#include <QVariantMap>
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
    auto* analyzer = BehaviorAnalytics::analyzer();
    if (!analyzer) return;

    auto result = analyzer->analyzeDialog(start, end);
    QVariantList dialogs = result.data["dialogs"].toList();

    std::sort(dialogs.begin(), dialogs.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap()["open_count"].toInt() > b.toMap()["open_count"].toInt();
    });

    table_->setRowCount(dialogs.size());
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

    for (int i = 0; i < dialogs.size(); ++i) {
        QVariantMap m = dialogs[i].toMap();
        table_->setItem(i, 0, new QTableWidgetItem(m["class"].toString()));
        table_->setItem(i, 1, new QTableWidgetItem(QString::number(m["open_count"].toInt())));
        table_->setItem(i, 2, new QTableWidgetItem(fmtMs(m["avg_duration_ms"].toInt())));
        table_->setItem(i, 3, new QTableWidgetItem(fmtMs(m["median_duration_ms"].toInt())));
        table_->setItem(i, 4, new QTableWidgetItem(fmtMs(m["min_duration_ms"].toInt())));
        table_->setItem(i, 5, new QTableWidgetItem(fmtMs(m["max_duration_ms"].toInt())));
        table_->setItem(i, 6, new QTableWidgetItem(m["instant_close_rate"].toString()));
    }

    // Scatter plot
    scatter_->detachItems();
    QVector<double> sx, sy;
    QStringList sLabels;
    double maxX = 0, maxY = 0;
    for (const auto& item : dialogs) {
        QVariantMap m = item.toMap();
        int cnt = m["open_count"].toInt();
        double avgSec = m["avg_duration_ms"].toDouble() / 1000.0;
        if (cnt <= 0) continue;
        sx << cnt; sy << avgSec;
        QString label = m["class"].toString();
        if (label.length() > 15) label = label.left(12) + "...";
        sLabels << label;
        maxX = qMax(maxX, (double)cnt); maxY = qMax(maxY, avgSec);
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
