#pragma once

#include <QWidget>
#include <QDateTime>
#include <QTableWidget>
#include <qwt_plot.h>

class DialogTab : public QWidget {
    Q_OBJECT
public:
    explicit DialogTab(QWidget* parent = nullptr);
    void updateData(const QDateTime& start, const QDateTime& end);

private:
    QTableWidget* table_ = nullptr;
    QwtPlot* scatter_ = nullptr;
};
