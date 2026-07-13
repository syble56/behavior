#pragma once

#include <QWidget>
#include <QDateTime>
#include <qwt_plot.h>

class OperationsTab : public QWidget {
    Q_OBJECT
public:
    explicit OperationsTab(QWidget* parent = nullptr);
    void updateData(const QDateTime& start, const QDateTime& end);

private:
    QwtPlot* chart_ = nullptr;
};
