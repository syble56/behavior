#pragma once

#include <QWidget>
#include <QDateTime>
#include "chart_widgets.h"

class InputTab : public QWidget {
    Q_OBJECT
public:
    explicit InputTab(QWidget* parent = nullptr);
    void updateData(const QDateTime& start, const QDateTime& end);

private:
    PieChartWidget* chart_ = nullptr;
};
