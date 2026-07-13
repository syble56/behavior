#include "chart_widgets.h"

#include <QPainter>
#include <QRectF>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QRegExp>
#include <QLinearGradient>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <qwt_plot_grid.h>
#include <qwt_column_symbol.h>
#include <qwt_scale_div.h>

// ============ ActionScaleDraw ============

ActionScaleDraw::ActionScaleDraw(const QStringList& labels)
    : labels_(labels) {
    enableComponent(QwtScaleDraw::Ticks, false);
    enableComponent(QwtScaleDraw::Backbone, false);
    isDateFormat_ = !labels.isEmpty()
                    && labels.first().contains(QRegExp("^\\d{4}-\\d{2}-\\d{2}$"));
}

QwtText ActionScaleDraw::label(double value) const {
    int idx = static_cast<int>(value + 0.5);
    if (idx < 0 || idx >= labels_.size())
        return QwtText();

    QString name = labels_[idx];
    if (isDateFormat_) {
        QStringList parts = name.split('-');
        if (parts.size() == 3)
            name = parts[1] + "-" + parts[2];
    } else {
        if (name.length() > 8)
            name = name.left(6) + "..";
    }
    return QwtText(name, QwtText::PlainText);
}

// ============ stylePlot ============

void stylePlot(QwtPlot* plot) {
    plot->setCanvasBackground(QColor("#3C3F44"));
    plot->setContentsMargins(8, 8, 8, 8);

    QFont axisFont("Microsoft YaHei", 9);
    QFont titleFont("Microsoft YaHei", 10, QFont::DemiBold);
    plot->setAxisFont(QwtPlot::xBottom, axisFont);
    plot->setAxisFont(QwtPlot::yLeft, axisFont);

    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        QwtText title = plot->axisTitle(axis);
        title.setFont(titleFont);
        title.setColor(QColor("#E2E8F0"));
        plot->setAxisTitle(axis, title);

        QwtScaleDraw* sd = plot->axisScaleDraw(axis);
        if (sd) {
            sd->setTickLength(QwtScaleDiv::MajorTick, (axis == QwtPlot::xBottom) ? 0 : 4);
            sd->setTickLength(QwtScaleDiv::MinorTick, 0);
        }
    }

    auto* grid = new QwtPlotGrid();
    grid->setMajorPen(QPen(QColor("#4A4D52"), 1, Qt::DotLine));
    grid->setMinorPen(QPen(Qt::NoPen));
    grid->attach(plot);

    plot->setStyleSheet("QwtPlot { border: none; } QwtScaleWidget { color: #CBD5E1; }");
}

// ============ styleBarChart ============

void styleBarChart(QwtPlotBarChart* chart, const QColor& baseColor) {
    auto* sym = new QwtColumnSymbol(QwtColumnSymbol::Box);
    sym->setLineWidth(0);
    sym->setPalette(QPalette(baseColor));
    chart->setSymbol(sym);
    chart->setMargin(3);
}

// ============ PieChartWidget ============

PieChartWidget::PieChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(300, 300);
}

void PieChartWidget::setData(const QStringList& labels, const QVector<double>& values) {
    labels_ = labels;
    values_ = values;
    update();
}

void PieChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width(), h = height();
    int side = qMin(w, h) - 40;
    int cx = w / 2, cy = h / 2;
    int radius = side / 2;

    double total = 0;
    for (double v : values_) total += v;
    if (total <= 0) {
        p.setPen(QColor("#94A3B8"));
        p.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }

    static const QColor colors[] = {
        QColor("#3B82F6"), QColor("#EF4444"), QColor("#10B981"),
        QColor("#F59E0B"), QColor("#8B5CF6"), QColor("#EC4899"),
        QColor("#06B6D4"), QColor("#84CC16")
    };

    double startAngle = 90.0 * 16;
    QRectF pieRect(cx - radius, cy - radius, radius * 2, radius * 2);

    for (int i = 0; i < values_.size(); ++i) {
        double span = values_[i] / total * 360.0 * 16;
        p.setBrush(colors[i % 8]);
        p.setPen(QPen(Qt::white, 2));
        p.drawPie(pieRect, static_cast<int>(startAngle), static_cast<int>(span));
        startAngle += span;
    }

    int legendX = cx + radius + 20;
    int legendY = cy - values_.size() * 22 / 2;
    p.setFont(QFont("Microsoft YaHei", 9));
    for (int i = 0; i < values_.size(); ++i) {
        QRectF colorRect(legendX, legendY + i * 22, 14, 14);
        p.setBrush(colors[i % 8]);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(colorRect, 3, 3);
        p.setPen(QColor("#E2E8F0"));
        QString pct = QString("%1 (%2%)").arg(labels_[i])
            .arg(qRound(values_[i] / total * 100));
        p.drawText(QPoint(legendX + 20, legendY + i * 22 + 12), pct);
    }
}

// ============ PathWidget ============

PathWidget::PathWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 300);
}

void PathWidget::setData(const QList<QPointF>& points, int screenWidth, int screenHeight) {
    points_ = points;
    screenW_ = screenWidth;
    screenH_ = screenHeight;
    update();
}

void PathWidget::clear() {
    points_.clear();
    update();
}

void PathWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(45, 30, -25, -35);

    p.setFont(QFont("Microsoft YaHei", 11, QFont::DemiBold));
    p.setPen(QColor("#E2E8F0"));
    p.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop, "Operation Trajectory");

    if (points_.isEmpty()) {
        p.setPen(QColor("#94A3B8"));
        p.setFont(QFont("Microsoft YaHei", 10));
        p.drawText(rect(), Qt::AlignCenter,
            "Click a day in the chart above to view trajectory");
        return;
    }

    // Compute bounds — use full screen dimensions if available
    qreal minX, maxX, minY, maxY;
    if (screenW_ > 0 && screenH_ > 0) {
        minX = 0; maxX = screenW_;
        minY = 0; maxY = screenH_;
    } else {
        // Fallback: auto-compute from data points
        minX = points_[0].x(); maxX = points_[0].x();
        minY = points_[0].y(); maxY = points_[0].y();
        for (const auto& pt : points_) {
            minX = qMin(minX, pt.x()); maxX = qMax(maxX, pt.x());
            minY = qMin(minY, pt.y()); maxY = qMax(maxY, pt.y());
        }
        qreal rangeX = maxX - minX; if (rangeX < 1) rangeX = 100;
        qreal rangeY = maxY - minY; if (rangeY < 1) rangeY = 100;
        minX = qMax(0.0, minX - rangeX * 0.05); maxX += rangeX * 0.05;
        minY = qMax(0.0, minY - rangeY * 0.05); maxY += rangeY * 0.05;
    }

    // Map screen coords → widget coords (screen Y: 0=top, increases downward)
    auto mapPt = [&](const QPointF& pt) -> QPointF {
        qreal x = r.left() + (pt.x() - minX) / (maxX - minX) * r.width();
        qreal y = r.top() + (pt.y() - minY) / (maxY - minY) * r.height();
        return QPointF(x, y);
    };

    int n = points_.size();

    // Helper: color at time fraction t (0=first, 1=last)
    auto colorAt = [](qreal t) -> QColor {
        return QColor(
            static_cast<int>(59 + (239 - 59) * t),
            static_cast<int>(130 + (68 - 130) * t),
            static_cast<int>(246 + (68 - 246) * t));
    };

    // Draw connecting lines with arrowheads
    for (int i = 1; i < n; ++i) {
        QPointF p1 = mapPt(points_[i - 1]);
        QPointF p2 = mapPt(points_[i]);
        qreal t = (qreal)i / n;
        QColor lineColor = colorAt(t);
        lineColor.setAlpha(200);
        p.setPen(QPen(lineColor, 1.5));
        p.drawLine(p1, p2);

        // Draw arrowhead at p2
        QLineF line(p1, p2);
        if (line.length() > 10) {
            double angle = line.angle() * M_PI / 180.0;
            qreal ax = p2.x() - 7 * cos(angle);
            qreal ay = p2.y() + 7 * sin(angle);
            QPointF arrowL(ax - 4 * cos(angle - M_PI / 6),
                           ay + 4 * sin(angle - M_PI / 6));
            QPointF arrowR(ax - 4 * cos(angle + M_PI / 6),
                           ay + 4 * sin(angle + M_PI / 6));
            p.setBrush(lineColor);
            p.setPen(Qt::NoPen);
            QPolygonF arrow;
            arrow << p2 << arrowL << arrowR;
            p.drawPolygon(arrow);
        }
    }

    // Draw points on top
    bool showNumbers = (n <= 30);
    for (int i = 0; i < n; ++i) {
        QPointF pt = mapPt(points_[i]);
        qreal t = (n > 1) ? (qreal)i / (n - 1) : 0.0;
        QColor color = colorAt(t);
        p.setBrush(color);
        p.setPen(QPen(QColor("#FFFFFF"), 1));
        p.drawEllipse(pt, 5, 5);

        // Show sequence number for first/last and when few points
        if (showNumbers || i == 0 || i == n - 1) {
            p.setPen(QColor("#FFFFFF"));
            p.setFont(QFont("Microsoft YaHei", 8, QFont::Bold));
            p.drawText(QRectF(pt.x() + 7, pt.y() - 8, 24, 16),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(i + 1));
        }
    }

    // Draw gradient legend (Start → End)
    {
        int legW = 120, legH = 10;
        int legX = r.right() - legW;
        int legY = r.top() + 4;

        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0, colorAt(0.0));
        gradient.setColorAt(1.0, colorAt(1.0));
        p.setBrush(QBrush(gradient));
        p.setPen(Qt::NoPen);
        p.drawRect(QRect(legX, legY, legW, legH));

        p.setPen(QColor("#94A3B8"));
        p.setFont(QFont("Microsoft YaHei", 8));
        p.drawRect(QRect(legX, legY, legW, legH));
        p.drawText(QRectF(legX - 30, legY - 2, 28, legH + 4),
                   Qt::AlignRight | Qt::AlignVCenter, "Start");
        p.drawText(QRectF(legX + legW + 4, legY - 2, 30, legH + 4),
                   Qt::AlignLeft | Qt::AlignVCenter, "End");
    }

    // Axis labels
    p.setPen(QColor("#94A3B8"));
    p.setFont(QFont("Microsoft YaHei", 9));
    p.drawText(r.left(), r.bottom() + 15, QString::number(static_cast<int>(minX)));
    p.drawText(r.right() - 40, r.bottom() + 15, QString::number(static_cast<int>(maxX)));
    p.drawText(r.left() - 38, r.top() + 10, QString::number(static_cast<int>(minY)));
    p.drawText(r.left() - 38, r.bottom(), QString::number(static_cast<int>(maxY)));
    p.drawText(r.right() + 5, r.bottom() + 15, "X");
    p.drawText(r.left() - 42, r.top() - 5, "Y");
}

// ============ HeatmapWidget ============

HeatmapWidget::HeatmapWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 400);
}

void HeatmapWidget::setData(const QMap<int, int>& regions, int maxCount) {
    regions_ = regions;
    maxCount_ = qMax(1, maxCount);
    update();
}

void HeatmapWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(50, 40, -20, -40);
    int cellW = r.width() / 10;
    int cellH = r.height() / 10;

    p.setFont(QFont("Microsoft YaHei", 11, QFont::DemiBold));
    p.setPen(QColor("#E2E8F0"));
    p.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop, "Main Window Click Heatmap");

    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 10; ++col) {
            int region = row * 10 + col;
            int count = regions_.value(region, 0);
            double intensity = (maxCount_ > 0) ? (double)count / maxCount_ : 0;
            QColor color;
            if (count == 0) {
                color = QColor("#3C3F44");
            } else {
                int rr = static_cast<int>(59 + (239 - 59) * intensity);
                int gg = static_cast<int>(130 + (68 - 130) * intensity);
                int bb = static_cast<int>(246 + (68 - 246) * intensity);
                color = QColor(rr, gg, bb);
            }
            QRect cell(r.left() + col * cellW, r.top() + row * cellH, cellW, cellH);
            p.fillRect(cell, color);
            p.setPen(QPen(QColor("#4A4D52"), 1));
            p.drawRect(cell);
            if (count > 0) {
                p.setPen(intensity > 0.5 ? Qt::white : QColor("#E2E8F0"));
                p.setFont(QFont("Microsoft YaHei", 8));
                p.drawText(cell, Qt::AlignCenter, QString::number(count));
            }
        }
    }

    p.setPen(QColor("#94A3B8"));
    p.setFont(QFont("Microsoft YaHei", 9));
    for (int i = 0; i < 10; ++i) {
        p.drawText(r.left() + i * cellW + cellW/2 - 5, r.bottom() + 15, QString::number(i));
        p.drawText(r.left() - 20, r.top() + i * cellH + cellH/2 + 5, QString::number(i));
    }
    p.drawText(r.left() - 35, r.top() + r.height()/2, "Row");
    p.drawText(r.left() + r.width()/2 - 10, r.bottom() + 30, "Col");
}
