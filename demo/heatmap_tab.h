#pragma once

#include <QWidget>
#include <QDateTime>
#include "chart_widgets.h"

class HeatmapTab : public QWidget {
    Q_OBJECT
public:
    explicit HeatmapTab(QWidget* parent = nullptr);
    void updateData(const QDateTime& start, const QDateTime& end);

private:
    HeatmapWidget* widget_ = nullptr;
};
