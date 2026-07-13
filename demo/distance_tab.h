#pragma once

#include <QWidget>
#include <QMap>
#include <QPointF>
#include <QStringList>
#include <QDateTime>
#include <QLabel>

#include <qwt_plot.h>

#include "chart_widgets.h"

class DistanceTab : public QWidget {
    Q_OBJECT
public:
    explicit DistanceTab(QWidget* parent = nullptr);

    // Update with new date range; queries DB internally
    void updateData(const QDateTime& start, const QDateTime& end);

signals:
    // Emitted after update; provides overall avg distance for stat card
    void avgDistanceChanged(int avgPx);

private slots:
    void onChartClicked(const QPointF& pos);

private:
    QwtPlot* chart_ = nullptr;
    PathWidget* pathWidget_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    QStringList dates_;                           // x-axis index → date
    QMap<QString, QList<QPointF>> trajectoryData_; // date → click points
    int screenW_ = 0;
    int screenH_ = 0;

    void selectDay(int index);
    void queryScreenSize();
};
