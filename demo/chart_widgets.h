#pragma once

#include <QWidget>
#include <QStringList>
#include <QVector>
#include <QMap>

#include <qwt_plot.h>
#include <qwt_plot_barchart.h>
#include <qwt_scale_draw.h>
#include <qwt_text.h>

// Custom X-axis scale draw — shows action names or dates
class ActionScaleDraw : public QwtScaleDraw {
public:
    explicit ActionScaleDraw(const QStringList& labels);

    QwtText label(double value) const override;

private:
    QStringList labels_;
    bool isDateFormat_ = false;
};

// Apply consistent dark-theme styling to a QwtPlot
void stylePlot(QwtPlot* plot);

// Apply bar chart styling with given base color
void styleBarChart(QwtPlotBarChart* chart, const QColor& baseColor = QColor("#3B82F6"));

// Simple pie chart widget
class PieChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit PieChartWidget(QWidget* parent = nullptr);

    void setData(const QStringList& labels, const QVector<double>& values);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QStringList labels_;
    QVector<double> values_;
};

// Operation trajectory widget — draws click points connected by lines
class PathWidget : public QWidget {
    Q_OBJECT
public:
    explicit PathWidget(QWidget* parent = nullptr);

    void setData(const QList<QPointF>& points, int screenWidth = 0, int screenHeight = 0);
    void clear();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QList<QPointF> points_;
    int screenW_ = 0;
    int screenH_ = 0;
};

// Simple heatmap widget (10x10 grid)
class HeatmapWidget : public QWidget {
    Q_OBJECT
public:
    explicit HeatmapWidget(QWidget* parent = nullptr);

    void setData(const QMap<int, int>& regions, int maxCount);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QMap<int, int> regions_;
    int maxCount_ = 1;
};
