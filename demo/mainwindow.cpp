#include "mainwindow.h"
#include "core/behavior_analytics.h"
#include "core/types.h"
#include "analyzer/behavior_analyzer.h"
#include "storage/database.h"
#include "storage/aggregator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QAction>
#include <QKeySequence>
#include <QDateTime>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QRegExp>
#include <QDebug>
#include <QMessageBox>
#include <QMenuBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <QToolBar>
#include <QStatusBar>
#include <QGroupBox>
#include <QSplitter>
#include <QFileDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QTimer>
#include <QThread>
#include <QPainter>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <algorithm>

// Qwt
#include <qwt_plot.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_histogram.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_grid.h>
#include <qwt_symbol.h>
#include <qwt_legend.h>
#include <qwt_column_symbol.h>
#include <qwt_series_data.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_draw.h>
#include <qwt_text.h>

using namespace ui_shared::behavior;

// 自定义 X 轴刻度绘制，显示操作名称或日期
class ActionScaleDraw : public QwtScaleDraw {
public:
    explicit ActionScaleDraw(const QStringList& labels) : labels_(labels) {
        enableComponent(QwtScaleDraw::Ticks, false);
        enableComponent(QwtScaleDraw::Backbone, false);
        
        // 检测是否为日期标签（格式：yyyy-MM-dd）
        isDateFormat_ = !labels.isEmpty() && labels.first().contains(QRegExp("^\\d{4}-\\d{2}-\\d{2}$"));
    }
    
    QwtText label(double value) const override {
        int idx = static_cast<int>(value + 0.5);  // 四舍五入
        if (idx >= 0 && idx < labels_.size()) {
            QString name = labels_[idx];
            
            if (isDateFormat_) {
                // 日期格式：显示为 MM-DD（去掉年份）
                QStringList parts = name.split('-');
                if (parts.size() == 3) {
                    name = parts[1] + "-" + parts[2];  // 显示 "06-24"
                }
            } else {
                // 操作名称：截断过长的名称
                if (name.length() > 8) {
                    name = name.left(6) + "..";
                }
            }
            
            return QwtText(name, QwtText::PlainText);
        }
        return QwtText();  // 返回空文本，不显示标签
    }
    
private:
    QStringList labels_;
    bool isDateFormat_ = false;
};

// 统一设置 QwtPlot 样式
void stylePlot(QwtPlot* plot) {
    plot->setCanvasBackground(QColor("#3C3F44"));
    plot->setContentsMargins(8, 8, 8, 8);
    
    // 字体
    QFont axisFont("Microsoft YaHei", 9);
    QFont titleFont("Microsoft YaHei", 10, QFont::DemiBold);
    plot->setAxisFont(QwtPlot::xBottom, axisFont);
    plot->setAxisFont(QwtPlot::yLeft, axisFont);
    
    // 轴标题 — 白色
    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        QwtText title = plot->axisTitle(axis);
        title.setFont(titleFont);
        title.setColor(QColor("#E2E8F0"));
        plot->setAxisTitle(axis, title);
    }
    
    // 轴刻度文字 — 浅灰白
    for (int axis = 0; axis < QwtPlot::axisCnt; ++axis) {
        QwtScaleDraw* sd = plot->axisScaleDraw(axis);
        if (sd) {
            sd->setTickLength(QwtScaleDiv::MajorTick, (axis == QwtPlot::xBottom) ? 0 : 4);
            sd->setTickLength(QwtScaleDiv::MinorTick, 0);
        }
    }
    
    // 网格 — 深灰虚线
    auto* grid = new QwtPlotGrid();
    grid->setMajorPen(QPen(QColor("#4A4D52"), 1, Qt::DotLine));
    grid->setMinorPen(QPen(Qt::NoPen));
    grid->attach(plot);
    
    plot->setStyleSheet("QwtPlot { border: none; } QwtScaleWidget { color: #CBD5E1; }");
}

// 给柱状图设置渐变色
void styleBarChart(QwtPlotBarChart* chart, const QColor& baseColor = QColor("#3B82F6")) {
    auto* sym = new QwtColumnSymbol(QwtColumnSymbol::Box);
    sym->setLineWidth(0);
    sym->setPalette(QPalette(baseColor));
    chart->setSymbol(sym);
    chart->setMargin(3);
}

// 简单的饼图绘制控件
class PieChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit PieChartWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(300, 300);
    }

    void setData(const QStringList& labels, const QVector<double>& values) {
        labels_ = labels;
        values_ = values;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
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
            p.drawText(rect(), Qt::AlignCenter, "无数据");
            return;
        }

        // 颜色列表 — 现代配色
        static const QColor colors[] = {
            QColor("#3B82F6"), QColor("#EF4444"), QColor("#10B981"),
            QColor("#F59E0B"), QColor("#8B5CF6"), QColor("#EC4899"),
            QColor("#06B6D4"), QColor("#84CC16")
        };

        // 画饼图
        double startAngle = 90.0 * 16;  // 从12点钟方向开始
        QRectF pieRect(cx - radius, cy - radius, radius * 2, radius * 2);

        for (int i = 0; i < values_.size(); ++i) {
            double span = values_[i] / total * 360.0 * 16;
            p.setBrush(colors[i % 8]);
            p.setPen(QPen(Qt::white, 2));
            p.drawPie(pieRect, static_cast<int>(startAngle), static_cast<int>(span));
            startAngle += span;
        }

        // 画图例
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

private:
    QStringList labels_;
    QVector<double> values_;
};

// 简单的热力图绘制控件
class HeatmapWidget : public QWidget {
    Q_OBJECT
public:
    explicit HeatmapWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(400, 400);
    }
    
    void setData(const QMap<int, int>& regions, int maxCount) {
        regions_ = regions;
        maxCount_ = qMax(1, maxCount);
        update();
    }
    
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        
        QRect r = rect().adjusted(50, 40, -20, -40);
        int cellW = r.width() / 10;
        int cellH = r.height() / 10;
        
        // 绘制标题
        p.setFont(QFont("Microsoft YaHei", 11, QFont::DemiBold));
        p.setPen(QColor("#E2E8F0"));
        p.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop, "Main Window Click Heatmap");
        
        // 绘制网格
        for (int row = 0; row < 10; ++row) {
            for (int col = 0; col < 10; ++col) {
                int region = row * 10 + col;
                int count = regions_.value(region, 0);
                
                // 计算颜色（从浅蓝到深红）
                double intensity = (maxCount_ > 0) ? (double)count / maxCount_ : 0;
                QColor color;
                if (count == 0) {
                    color = QColor("#3C3F44");
                } else {
                    // 从蓝色渐变到红色
                    int r = static_cast<int>(59 + (239 - 59) * intensity);
                    int g = static_cast<int>(130 + (68 - 130) * intensity);
                    int b = static_cast<int>(246 + (68 - 246) * intensity);
                    color = QColor(r, g, b);
                }
                
                QRect cell(r.left() + col * cellW, r.top() + row * cellH, cellW, cellH);
                p.fillRect(cell, color);
                p.setPen(QPen(QColor("#4A4D52"), 1));
                p.drawRect(cell);
                
                // 显示数值
                if (count > 0) {
                    p.setPen(intensity > 0.5 ? Qt::white : QColor("#E2E8F0"));
                    p.setFont(QFont("Microsoft YaHei", 8));
                    p.drawText(cell, Qt::AlignCenter, QString::number(count));
                }
            }
        }
        
        // 绘制坐标轴标签
        p.setPen(QColor("#94A3B8"));
        p.setFont(QFont("Microsoft YaHei", 9));
        for (int i = 0; i < 10; ++i) {
            // X轴（列）
            p.drawText(r.left() + i * cellW + cellW/2 - 5, r.bottom() + 15, QString::number(i));
            // Y轴（行）
            p.drawText(r.left() - 20, r.top() + i * cellH + cellH/2 + 5, QString::number(i));
        }
        p.drawText(r.left() - 35, r.top() + r.height()/2, "Row");
        p.drawText(r.left() + r.width()/2 - 10, r.bottom() + 30, "Col");
    }
    
private:
    QMap<int, int> regions_;
    int maxCount_ = 1;
};

// 设置对话框
class SettingsDialog : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("系统设置");
        setMinimumWidth(400);
        auto* lay = new QVBoxLayout(this);
        
        auto* dbGroup = new QGroupBox("数据库设置", this);
        auto* dbLay = new QVBoxLayout(dbGroup);
        
        auto* pathLay = new QHBoxLayout();
        pathLay->addWidget(new QLabel("数据库路径:", this));
        auto* pathEdit = new QLineEdit(this);
        pathEdit->setPlaceholderText("默认路径");
        pathLay->addWidget(pathEdit);
        auto* browseBtn = new QPushButton("浏览...", this);
        pathLay->addWidget(browseBtn);
        dbLay->addLayout(pathLay);
        
        auto* retentionLay = new QHBoxLayout();
        retentionLay->addWidget(new QLabel("数据保留天数:", this));
        auto* retentionSpin = new QSpinBox(this);
        retentionSpin->setRange(1, 365);
        retentionSpin->setValue(180);
        retentionLay->addWidget(retentionSpin);
        retentionLay->addStretch();
        dbLay->addLayout(retentionLay);
        
        lay->addWidget(dbGroup);
        
        auto* collectGroup = new QGroupBox("采集设置", this);
        auto* collectLay = new QVBoxLayout(collectGroup);
        
        auto* batchLay = new QHBoxLayout();
        batchLay->addWidget(new QLabel("批量大小:", this));
        auto* batchSpin = new QSpinBox(this);
        batchSpin->setRange(10, 1000);
        batchSpin->setValue(100);
        batchLay->addWidget(batchSpin);
        batchLay->addStretch();
        collectLay->addLayout(batchLay);
        
        auto* enableCheck = new QCheckBox("启用行为采集", this);
        enableCheck->setChecked(true);
        collectLay->addWidget(enableCheck);
        
        lay->addWidget(collectGroup);
        
        auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        lay->addWidget(btns);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
};

// 系统按钮弹窗
class SystemButtonsDialog : public QDialog {
public:
    explicit SystemButtonsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("系统操作");
        setMinimumWidth(300);
        auto* lay = new QVBoxLayout(this);
        
        lay->addWidget(new QLabel("快捷操作:", this));
        
        auto* btnExport = new QPushButton("导出数据", this);
        btnExport->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
        lay->addWidget(btnExport);
        
        auto* btnImport = new QPushButton("导入数据", this);
        btnImport->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
        lay->addWidget(btnImport);
        
        auto* btnClear = new QPushButton("清空数据", this);
        btnClear->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
        lay->addWidget(btnClear);
        
        auto* btnBackup = new QPushButton("备份数据库", this);
        lay->addWidget(btnBackup);
        
        lay->addStretch();
        
        auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
        lay->addWidget(btns);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        connect(btnExport, &QPushButton::clicked, this, []{
            QMessageBox::information(nullptr, "导出", "数据导出功能");
        });
        connect(btnClear, &QPushButton::clicked, this, []{
            auto ret = QMessageBox::question(nullptr, "确认清空",
                "将清空所有操作记录和聚合数据，此操作不可撤销。\n\n确定要清空吗？",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ret != QMessageBox::Yes) return;

            auto& db = ui_shared::behavior::Database::instance();
            auto sqlDb = db.connection();

            // 清空所有数据表
            QStringList tables = {
                "operations", "sessions",
                "agg_operation_stats", "agg_module_stats", "agg_input_stats",
                "agg_heatmap_stats", "agg_dialog_stats", "agg_time_distribution"
            };
            for (const auto& t : tables) {
                QSqlQuery q(sqlDb);
                q.exec(QStringLiteral("DELETE FROM %1").arg(t));
            }

            QMessageBox::information(nullptr, "已完成", "所有数据已清空。");
        });
    }
};

// 分析结果对话框
class AnalysisDialog : public QDialog {
    Q_OBJECT
public:
    explicit AnalysisDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Behavior Analysis");
        setMinimumSize(1100, 800);
        
        // 应用数据仪表盘样式
        setStyleSheet(R"(
            QDialog {
                background-color: #32353A;
            }
            QGroupBox {
                font-weight: 600;
                font-size: 13px;
                color: #E2E8F0;
                border: 1px solid #4A4D52;
                border-radius: 8px;
                margin-top: 12px;
                padding-top: 10px;
                background-color: #3C3F44;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 2px 10px;
                background-color: #3C3F44;
                color: #CBD5E1;
            }
            QLabel {
                color: #E2E8F0;
                font-size: 13px;
            }
            QLabel#statCard {
                background-color: #3C3F44;
                border: 1px solid #4A4D52;
                border-radius: 8px;
                padding: 12px 16px;
                font-size: 13px;
                color: #94A3B8;
            }
            QLabel#statValue {
                font-size: 22px;
                font-weight: 700;
                color: #FFFFFF;
            }
            QLabel#statTitle {
                font-size: 11px;
                font-weight: 500;
                color: #94A3B8;
                letter-spacing: 1px;
            }
            QPushButton {
                background-color: #3C3F44;
                color: #E2E8F0;
                border: 1px solid #4A4D52;
                border-radius: 6px;
                padding: 7px 14px;
                font-size: 13px;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: #4A4D52;
                border-color: #64748B;
            }
            QPushButton:pressed {
                background-color: #5A5D62;
            }
            QPushButton:default {
                background-color: #2563EB;
                color: white;
                border-color: #2563EB;
            }
            QPushButton:default:hover {
                background-color: #1D4ED8;
            }
            QDateTimeEdit {
                background-color: #3C3F44;
                border: 1px solid #4A4D52;
                border-radius: 6px;
                padding: 5px 10px;
                color: #FFFFFF;
                font-size: 13px;
            }
            QDateTimeEdit:focus {
                border-color: #3B82F6;
            }
            QTabWidget::pane {
                border: 1px solid #4A4D52;
                background-color: #3C3F44;
                border-radius: 8px;
                top: -1px;
            }
            QTabBar::tab {
                background-color: #32353A;
                color: #94A3B8;
                border: 1px solid #4A4D52;
                border-bottom: none;
                padding: 9px 18px;
                margin-right: 3px;
                border-top-left-radius: 6px;
                border-top-right-radius: 6px;
                font-size: 13px;
                font-weight: 500;
            }
            QTabBar::tab:selected {
                background-color: #3C3F44;
                color: #FFFFFF;
                border-bottom-color: #3C3F44;
                font-weight: 600;
            }
            QTabBar::tab:hover:!selected {
                background-color: #4A4D52;
                color: #E2E8F0;
            }
            QTableWidget {
                background-color: #3C3F44;
                alternate-background-color: #34373C;
                border: none;
                gridline-color: #4A4D52;
                font-size: 13px;
                border-radius: 8px;
            }
            QTableWidget::item {
                padding: 8px 6px;
                color: #E2E8F0;
            }
            QTableWidget::item:selected {
                background-color: #2563EB;
                color: #FFFFFF;
            }
            QTableWidget::item:hover {
                background-color: #4A4D52;
            }
            QHeaderView::section {
                background-color: #32353A;
                color: #CBD5E1;
                font-weight: 600;
                font-size: 12px;
                padding: 10px 8px;
                border: none;
                border-bottom: 2px solid #4A4D52;
            }
            QHeaderView::section:vertical {
                background-color: #32353A;
                color: #94A3B8;
                border-right: 1px solid #4A4D52;
                border-bottom: none;
            }
            QScrollBar:vertical {
                background-color: #32353A;
                width: 10px;
                border-radius: 5px;
                margin: 0;
            }
            QScrollBar::handle:vertical {
                background-color: #64748B;
                border-radius: 5px;
                min-height: 30px;
            }
            QScrollBar::handle:vertical:hover {
                background-color: #94A3B8;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
            QScrollBar:horizontal {
                background-color: #32353A;
                height: 10px;
                border-radius: 5px;
                margin: 0;
            }
            QScrollBar::handle:horizontal {
                background-color: #64748B;
                border-radius: 5px;
                min-width: 30px;
            }
            QScrollBar::handle:horizontal:hover {
                background-color: #94A3B8;
            }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
                width: 0;
            }
        )");
        
        auto* lay = new QVBoxLayout(this);
        lay->setSpacing(12);
        lay->setContentsMargins(16, 16, 16, 16);
        
        // 时间范围选择（只显示日期）
        auto* timeGroup = new QGroupBox("Date Range", this);
        auto* timeLay = new QHBoxLayout(timeGroup);
        timeLay->setSpacing(8);
        
        timeLay->addWidget(new QLabel("Start:", this));
        startDate_ = new QDateEdit(this);
        startDate_->setCalendarPopup(true);
        startDate_->setDate(QDate::currentDate().addDays(-7));
        startDate_->setDisplayFormat("yyyy-MM-dd");
        timeLay->addWidget(startDate_);
        
        timeLay->addSpacing(12);
        timeLay->addWidget(new QLabel("End:", this));
        endDate_ = new QDateEdit(this);
        endDate_->setCalendarPopup(true);
        endDate_->setDate(QDate::currentDate());
        endDate_->setDisplayFormat("yyyy-MM-dd");
        timeLay->addWidget(endDate_);
        
        timeLay->addSpacing(16);
        
        // 快捷按钮
        auto* btnToday = new QPushButton("Today", this);
        auto* btnLast3Days = new QPushButton("Last 3 Days", this);
        auto* btnLastWeek = new QPushButton("Last 7 Days", this);
        auto* btnLastMonth = new QPushButton("Last 30 Days", this);
        timeLay->addWidget(btnToday);
        timeLay->addWidget(btnLast3Days);
        timeLay->addWidget(btnLastWeek);
        timeLay->addWidget(btnLastMonth);
        
        timeLay->addSpacing(8);
        
        auto* btnAnalyze = new QPushButton("Analyze", this);
        btnAnalyze->setDefault(true);
        btnAnalyze->setStyleSheet(R"(
            QPushButton {
                background-color: #2563EB;
                color: white;
                border: none;
                border-radius: 6px;
                padding: 8px 24px;
                font-weight: 600;
                font-size: 13px;
            }
            QPushButton:hover {
                background-color: #1D4ED8;
            }
            QPushButton:pressed {
                background-color: #1E40AF;
            }
        )");
        timeLay->addWidget(btnAnalyze);
        
        timeLay->addStretch();
        
        lay->addWidget(timeGroup);
        
        // 汇总统计卡片
        auto* cardLay = new QHBoxLayout();
        cardLay->setSpacing(10);
        
        auto* card1 = new QLabel(this);
        card1->setObjectName("statCard");
        card1->setText("<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Total Events</span></td></tr><tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>0</span></td></tr></table>");
        
        auto* card2 = new QLabel(this);
        card2->setObjectName("statCard");
        card2->setText("<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Active Days</span></td></tr><tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>0</span></td></tr></table>");
        
        auto* card3 = new QLabel(this);
        card3->setObjectName("statCard");
        card3->setText("<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Top Module</span></td></tr><tr><td><span style='font-size:16px;font-weight:600;color:#FFFFFF;'>-</span></td></tr></table>");
        
        auto* card4 = new QLabel(this);
        card4->setObjectName("statCard");
        card4->setText("<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Dialog Opens</span></td></tr><tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>0</span></td></tr></table>");
        
        statCards_ = {card1, card2, card3, card4};
        for (auto* card : statCards_) {
            card->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            card->setMinimumHeight(62);
            card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            cardLay->addWidget(card);
        }
        lay->addLayout(cardLay);
        
        // 结果显示区域（使用 tab）
        tabWidget_ = new QTabWidget(this);
        
        // 操作频率 tab - 柱状图
        operationChart_ = new QwtPlot(this);
        stylePlot(operationChart_);
        operationChart_->setAxisTitle(QwtPlot::yLeft, "Count");
        operationChart_->setAxisTitle(QwtPlot::xBottom, "Action");
        tabWidget_->addTab(operationChart_, "Operations");
        
        // 模块统计 tab - 柱状图
        moduleChart_ = new QwtPlot(this);
        stylePlot(moduleChart_);
        moduleChart_->setAxisTitle(QwtPlot::yLeft, "Count");
        moduleChart_->setAxisTitle(QwtPlot::xBottom, "Module");
        tabWidget_->addTab(moduleChart_, "Modules");
        
        // 输入方式 tab - 饼图
        inputChart_ = new PieChartWidget(this);
        tabWidget_->addTab(inputChart_, "Input Methods");
        
        // 时间分布 tab - 折线图
        timeChart_ = new QwtPlot(this);
        stylePlot(timeChart_);
        timeChart_->setAxisTitle(QwtPlot::yLeft, "Count");
        timeChart_->setAxisTitle(QwtPlot::xBottom, "Date");
        tabWidget_->addTab(timeChart_, "Time Distribution");
        
        // 热力图 tab
        heatmapWidget_ = new HeatmapWidget(this);
        tabWidget_->addTab(heatmapWidget_, "Click Heatmap");
        
        // 对话框分析 tab - 表格
        dialogTable_ = new QTableWidget(this);
        dialogTable_->setColumnCount(7);
        dialogTable_->setHorizontalHeaderLabels({"Dialog", "Opens", "Avg Duration", "Median", "Min", "Max", "Instant Close %"});
        dialogTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        dialogTable_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
        dialogTable_->setAlternatingRowColors(true);
        dialogTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tabWidget_->addTab(dialogTable_, "Dialog Analysis");
        
        // 对话框散点图 tab - 频率 × 时长
        dialogScatter_ = new QwtPlot(this);
        stylePlot(dialogScatter_);
        dialogScatter_->setAxisTitle(QwtPlot::xBottom, "Opens");
        dialogScatter_->setAxisTitle(QwtPlot::yLeft, "Avg Duration (s)");
        dialogScatter_->setMinimumSize(500, 400);
        tabWidget_->addTab(dialogScatter_, "Dialog Distribution");
        
        lay->addWidget(tabWidget_);
        
        // 底部按钮
        auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
        lay->addWidget(btns);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        // 连接信号
        connect(btnToday, &QPushButton::clicked, this, [this]{
            endDate_->setDate(QDate::currentDate());
            startDate_->setDate(QDate::currentDate());
        });
        connect(btnLast3Days, &QPushButton::clicked, this, [this]{
            endDate_->setDate(QDate::currentDate());
            startDate_->setDate(QDate::currentDate().addDays(-2));
        });
        connect(btnLastWeek, &QPushButton::clicked, this, [this]{
            endDate_->setDate(QDate::currentDate());
            startDate_->setDate(QDate::currentDate().addDays(-6));
        });
        connect(btnLastMonth, &QPushButton::clicked, this, [this]{
            endDate_->setDate(QDate::currentDate());
            startDate_->setDate(QDate::currentDate().addDays(-29));
        });
        connect(btnAnalyze, &QPushButton::clicked, this, &AnalysisDialog::onAnalyze);
        
        // 弹窗打开时自动执行一次统计
        QTimer::singleShot(100, this, &AnalysisDialog::onAnalyze);
    }
    
private slots:
    void onAnalyze() {
        // 获取日期范围（开始时间为当天 00:00:00，结束时间为当天 23:59:59）
        QDateTime start(startDate_->date(), QTime(0, 0, 0));
        QDateTime end(endDate_->date(), QTime(23, 59, 59));
        
        auto& db = ui_shared::behavior::Database::instance();
        QSqlDatabase sqlDb = db.connection();
        if (!sqlDb.isOpen()) {
            QMessageBox::warning(this, "Error", "Database not open");
            return;
        }
        
        // 显示调试信息：数据库中的数据时间范围
        qint64 startMs = start.toMSecsSinceEpoch();
        qint64 endMs = end.toMSecsSinceEpoch();
        
        QSqlQuery debugQuery(sqlDb);
        debugQuery.prepare("SELECT COUNT(*) FROM operations WHERE time >= ? AND time < ?");
        debugQuery.addBindValue(startMs);
        debugQuery.addBindValue(endMs);
        debugQuery.exec();
        int countInRange = 0;
        if (debugQuery.next()) countInRange = debugQuery.value(0).toInt();
        
        debugQuery.exec("SELECT COUNT(*) FROM operations");
        int totalOps = 0;
        if (debugQuery.next()) totalOps = debugQuery.value(0).toInt();
        
        debugQuery.exec("SELECT MIN(time), MAX(time) FROM operations");
        qint64 minTime = 0, maxTime = 0;
        if (debugQuery.next()) {
            minTime = debugQuery.value(0).toLongLong();
            maxTime = debugQuery.value(1).toLongLong();
        }
        
        QString debugInfo = QString("统计区间: %1 至 %2\n")
            .arg(start.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(end.toString("yyyy-MM-dd HH:mm:ss"));
        debugInfo += QString("区间内记录数: %1\n").arg(countInRange);
        debugInfo += QString("总记录数: %1\n").arg(totalOps);
        if (minTime > 0 && maxTime > 0) {
            debugInfo += QString("数据时间范围: %1 至 %2")
                .arg(QDateTime::fromMSecsSinceEpoch(minTime).toString("yyyy-MM-dd HH:mm:ss"))
                .arg(QDateTime::fromMSecsSinceEpoch(maxTime).toString("yyyy-MM-dd HH:mm:ss"));
        } else {
            debugInfo += "数据时间范围: (无数据)";
        }
        
        qDebug() << "Analysis Debug Info:\n" << debugInfo;
        
        // 检查聚合表数据
        QSqlQuery aggCheckQuery(sqlDb);
        aggCheckQuery.exec("SELECT COUNT(*) FROM agg_operation_stats");
        int aggCount = 0;
        if (aggCheckQuery.next()) aggCount = aggCheckQuery.value(0).toInt();
        
        aggCheckQuery.exec("SELECT MIN(time_bucket), MAX(time_bucket) FROM agg_operation_stats");
        QString aggMinBucket, aggMaxBucket;
        if (aggCheckQuery.next()) {
            aggMinBucket = aggCheckQuery.value(0).toString();
            aggMaxBucket = aggCheckQuery.value(1).toString();
        }
        
        qDebug() << "Aggregation table: count=" << aggCount 
                 << ", range=" << aggMinBucket << "to" << aggMaxBucket;
        
        // 更新窗口标题，显示数据情况
        QString title = QString("Behavior Analysis - Range: %1, Total: %2, Aggregated: %3")
            .arg(countInRange).arg(totalOps).arg(aggCount);
        setWindowTitle(title);
        
        // 更新汇总卡片
        {
            // 活跃天数
            QSqlQuery activeDayQuery(sqlDb);
            activeDayQuery.prepare(
                "SELECT COUNT(DISTINCT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime'))) "
                "FROM operations WHERE time >= ? AND time < ?");
            activeDayQuery.addBindValue(startMs);
            activeDayQuery.addBindValue(endMs);
            activeDayQuery.exec();
            int activeDays = 0;
            if (activeDayQuery.next()) activeDays = activeDayQuery.value(0).toInt();
            
            // 主要模块
            QSqlQuery topModQuery(sqlDb);
            topModQuery.prepare(
                "SELECT COALESCE(NULLIF(window_class,''), 'unknown'), COUNT(*) as cnt "
                "FROM operations WHERE time >= ? AND time < ? "
                "GROUP BY COALESCE(NULLIF(window_class,''), 'unknown') ORDER BY cnt DESC LIMIT 1");
            topModQuery.addBindValue(startMs);
            topModQuery.addBindValue(endMs);
            topModQuery.exec();
            QString topMod = "-";
            if (topModQuery.next()) topMod = topModQuery.value(0).toString();
            if (topMod.length() > 20) topMod = topMod.left(17) + "...";
            
            // 对话框打开次数
            QSqlQuery dlgQuery(sqlDb);
            dlgQuery.prepare(
                "SELECT COUNT(*) FROM operations WHERE time >= ? AND time < ? AND event_type='dialog_open'");
            dlgQuery.addBindValue(startMs);
            dlgQuery.addBindValue(endMs);
            dlgQuery.exec();
            int dlgCount = 0;
            if (dlgQuery.next()) dlgCount = dlgQuery.value(0).toInt();
            
            statCards_[0]->setText(QString(
                "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Total Events</span></td></tr>"
                "<tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>%1</span></td></tr></table>")
                .arg(countInRange));
            statCards_[1]->setText(QString(
                "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Active Days</span></td></tr>"
                "<tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>%1</span></td></tr></table>")
                .arg(activeDays));
            statCards_[2]->setText(QString(
                "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Top Module</span></td></tr>"
                "<tr><td><span style='font-size:16px;font-weight:600;color:#FFFFFF;'>%1</span></td></tr></table>")
                .arg(topMod));
            statCards_[3]->setText(QString(
                "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Dialog Opens</span></td></tr>"
                "<tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>%1</span></td></tr></table>")
                .arg(dlgCount));
        }
        
        // 格式化时间桶范围（天粒度）
        QString startBucket = start.toString("yyyy-MM-dd");
        QString endBucket = end.toString("yyyy-MM-dd");
        
        // ≤3个月：直接查 operations（实时准确，数据量可控）
        // >3个月：走聚合表（长期趋势，今天数据可能略滞后）
        int rangeDays = start.date().daysTo(end.date()) + 1;
        bool useAgg = false;
        if (rangeDays > 90) {
            // 检查聚合表天粒度是否覆盖查询范围
            QSqlQuery dayRangeQuery(sqlDb);
            dayRangeQuery.exec(
                "SELECT MIN(time_bucket), MAX(time_bucket) FROM agg_operation_stats "
                "WHERE length(time_bucket) = 10");
            QString aggMinDay, aggMaxDay;
            if (dayRangeQuery.next()) {
                aggMinDay = dayRangeQuery.value(0).toString();
                aggMaxDay = dayRangeQuery.value(1).toString();
            }
            if (!aggMinDay.isEmpty() && startBucket >= aggMinDay && endBucket <= aggMaxDay) {
                useAgg = true;
            }
        }
        
        qDebug() << "Using aggregation table:" << useAgg 
                 << ", query range:" << startBucket << "to" << endBucket
                 << ", range days:" << rangeDays;
        
        int total = 0;
        
        // ========== 操作频率图表 ==========
        operationChart_->detachItems();
        QVector<double> opData;
        QStringList opLabels;
        
        if (useAgg) {
            QSqlQuery opQuery(sqlDb);
            opQuery.prepare(
                "SELECT action_key, SUM(count) as cnt "
                "FROM agg_operation_stats "
                "WHERE time_bucket >= ? AND time_bucket <= ? "
                "GROUP BY action_key ORDER BY cnt DESC LIMIT 15");
            opQuery.addBindValue(startBucket);
            opQuery.addBindValue(endBucket);
            opQuery.exec();
            
            QSqlQuery totalQuery(sqlDb);
            totalQuery.prepare("SELECT SUM(count) FROM agg_operation_stats WHERE time_bucket >= ? AND time_bucket <= ?");
            totalQuery.addBindValue(startBucket);
            totalQuery.addBindValue(endBucket);
            totalQuery.exec();
            if (totalQuery.next()) total = totalQuery.value(0).toInt();
            
            while (opQuery.next()) {
                opLabels << opQuery.value(0).toString();
                opData << opQuery.value(1).toDouble();
            }
        } else {
            qint64 startMs = start.toMSecsSinceEpoch();
            qint64 endMs = end.toMSecsSinceEpoch();
            
            QSqlQuery opQuery(sqlDb);
            opQuery.prepare(
                "SELECT COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), event_type) as action, COUNT(*) as cnt "
                "FROM operations WHERE time >= ? AND time < ? GROUP BY action ORDER BY cnt DESC LIMIT 15");
            opQuery.addBindValue(startMs);
            opQuery.addBindValue(endMs);
            opQuery.exec();
            
            QSqlQuery totalQuery(sqlDb);
            totalQuery.prepare("SELECT COUNT(*) FROM operations WHERE time >= ? AND time < ?");
            totalQuery.addBindValue(startMs);
            totalQuery.addBindValue(endMs);
            totalQuery.exec();
            if (totalQuery.next()) total = totalQuery.value(0).toInt();
            
            while (opQuery.next()) {
                opLabels << opQuery.value(0).toString();
                opData << opQuery.value(1).toDouble();
            }
        }
        
        if (!opData.isEmpty()) {
            auto* opChart = new QwtPlotBarChart("Operations");
            styleBarChart(opChart, QColor("#3B82F6"));
            opChart->setSamples(opData);
            opChart->attach(operationChart_);
            operationChart_->setAxisScale(QwtPlot::xBottom, 0, opData.size() - 1, 1);
            operationChart_->setAxisScale(QwtPlot::yLeft, 0, *std::max_element(opData.begin(), opData.end()) * 1.2);
            operationChart_->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(opLabels));
            operationChart_->replot();
        }
        
        // ========== 模块统计图表 ==========
        moduleChart_->detachItems();
        QVector<double> modData;
        QStringList modLabels;
        
        if (useAgg) {
            QSqlQuery modQuery(sqlDb);
            modQuery.prepare(
                "SELECT module_class, SUM(count) as cnt "
                "FROM agg_module_stats WHERE time_bucket >= ? AND time_bucket <= ? "
                "GROUP BY module_class ORDER BY cnt DESC");
            modQuery.addBindValue(startBucket);
            modQuery.addBindValue(endBucket);
            modQuery.exec();
            
            while (modQuery.next()) {
                modLabels << modQuery.value(0).toString();
                modData << modQuery.value(1).toDouble();
            }
        } else {
            qint64 startMs = start.toMSecsSinceEpoch();
            qint64 endMs = end.toMSecsSinceEpoch();
            
            QSqlQuery modQuery(sqlDb);
            modQuery.prepare(
                "SELECT COALESCE(NULLIF(window_class,''), 'unknown') as module, COUNT(*) as cnt "
                "FROM operations WHERE time >= ? AND time < ? GROUP BY module ORDER BY cnt DESC");
            modQuery.addBindValue(startMs);
            modQuery.addBindValue(endMs);
            modQuery.exec();
            
            while (modQuery.next()) {
                modLabels << modQuery.value(0).toString();
                modData << modQuery.value(1).toDouble();
            }
        }
        
        if (!modData.isEmpty()) {
            auto* modChart = new QwtPlotBarChart("Modules");
            styleBarChart(modChart, QColor("#8B5CF6"));
            modChart->setSamples(modData);
            modChart->attach(moduleChart_);
            // 单个数据点时使用对称区间，避免 min==max 导致柱子塌缩成竖线
            if (modData.size() == 1) {
                moduleChart_->setAxisScale(QwtPlot::xBottom, -0.5, 0.5, 1);
            } else {
                moduleChart_->setAxisScale(QwtPlot::xBottom, 0, modData.size() - 1, 1);
            }
            moduleChart_->setAxisScale(QwtPlot::yLeft, 0, *std::max_element(modData.begin(), modData.end()) * 1.2);
            moduleChart_->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(modLabels));
            moduleChart_->replot();
        }
        
        // ========== 输入方式图表（通过分析器接口） ==========
        QVector<double> inputData;
        QStringList inputLabels;
        
        {
            auto* analyzer = BehaviorAnalytics::analyzer();
            if (analyzer) {
                auto inputResult = analyzer->analyzeInput(start, end);
                // 数据格式是 mouse/touch/keyboard/scroll/knob 五个对象
                QStringList types = {"mouse", "touch", "keyboard", "scroll", "knob"};
                QStringList typeLabels = {"Mouse", "Touch", "Keyboard", "Scroll", "Knob"};
                for (int i = 0; i < types.size(); ++i) {
                    QVariantMap m = inputResult.data[types[i]].toMap();
                    double v = m["count"].toDouble();
                    if (v > 0) {
                        inputLabels << typeLabels[i];
                        inputData << v;
                    }
                }
            }
        }
        
        if (!inputData.isEmpty()) {
            inputChart_->setData(inputLabels, inputData);
        } else {
            inputChart_->setData({}, {});
        }
        
        // ========== 时间分布图表（折线图） ==========
        timeChart_->detachItems();
        QVector<double> timeData;
        QStringList timeLabels;
        
        if (useAgg) {
            QSqlQuery timeQuery(sqlDb);
            timeQuery.prepare(
                "SELECT date, SUM(count) as cnt "
                "FROM agg_time_distribution WHERE date >= ? AND date <= ? "
                "GROUP BY date ORDER BY date");
            timeQuery.addBindValue(start.toString("yyyy-MM-dd"));
            timeQuery.addBindValue(end.toString("yyyy-MM-dd"));
            timeQuery.exec();
            
            while (timeQuery.next()) {
                timeLabels << timeQuery.value(0).toString();
                timeData << timeQuery.value(1).toDouble();
            }
        } else {
            qint64 startMs = start.toMSecsSinceEpoch();
            qint64 endMs = end.toMSecsSinceEpoch();
            
            QSqlQuery timeQuery(sqlDb);
            timeQuery.prepare(
                "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) as dt, COUNT(*) as cnt "
                "FROM operations WHERE time >= ? AND time < ? GROUP BY dt ORDER BY dt");
            timeQuery.addBindValue(startMs);
            timeQuery.addBindValue(endMs);
            timeQuery.exec();
            
            while (timeQuery.next()) {
                timeLabels << timeQuery.value(0).toString();
                timeData << timeQuery.value(1).toDouble();
            }
        }
        
        // 按选的日期范围补零，保证 X 轴完整
        {
            QHash<QString, double> dayMap;
            for (int i = 0; i < timeLabels.size(); ++i)
                dayMap[timeLabels[i]] = timeData[i];

            timeLabels.clear();
            timeData.clear();
            for (QDate d = start.date(); d <= end.date(); d = d.addDays(1)) {
                QString key = d.toString("yyyy-MM-dd");
                timeLabels << key;
                timeData << dayMap.value(key, 0.0);
            }
        }

        if (!timeData.isEmpty()) {
            int n = timeData.size();
            // 根据数据点数量选择图表类型
            if (n <= 7) {
                // 一周以内：柱状图
                auto* timeChart = new QwtPlotBarChart("Time Distribution");
                styleBarChart(timeChart, QColor("#10B981"));
                timeChart->setSamples(timeData);
                timeChart->attach(timeChart_);
                if (n == 1) {
                    timeChart_->setAxisScale(QwtPlot::xBottom, -0.5, 0.5, 1);
                } else {
                    timeChart_->setAxisScale(QwtPlot::xBottom, 0, n - 1, 1);
                }
            } else {
                // 超过一周：折线图
                auto* timeCurve = new QwtPlotCurve("Time Distribution");
                QVector<double> x(n);
                for (int i = 0; i < n; ++i) x[i] = i;
                timeCurve->setSamples(x.data(), timeData.data(), n);
                timeCurve->setPen(QPen(QColor("#2563EB"), 2));
                timeCurve->setBrush(QBrush(QColor(37, 99, 235, 40)));
                timeCurve->attach(timeChart_);
                // 智能标签间距：≤30天每5天一个标签，≤90天每10天，否则每30天
                int step = (n <= 30) ? 5 : (n <= 90) ? 10 : 30;
                timeChart_->setAxisScale(QwtPlot::xBottom, 0, n - 1, step);
            }
            
            // Y 轴：最大值至少为 1，避免全零时比例塌缩
            double maxVal = *std::max_element(timeData.begin(), timeData.end());
            timeChart_->setAxisScale(QwtPlot::yLeft, 0, qMax(maxVal * 1.2, 1.0));
            
            // 使用自定义刻度绘制，显示日期标签
            timeChart_->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(timeLabels));
            
            // 设置 X 轴标题，显示统计区间
            QString xAxisTitle = QString("Date (%1 to %2)")
                .arg(start.toString("yyyy-MM-dd"))
                .arg(end.toString("yyyy-MM-dd"));
            timeChart_->setAxisTitle(QwtPlot::xBottom, xAxisTitle);
            
            timeChart_->replot();
        } else {
            // 没有数据时显示提示
            timeChart_->setAxisTitle(QwtPlot::xBottom, "Date (no data)");
            timeChart_->replot();
        }
        
        // ========== 热力图 ==========
        QMap<int, int> heatData;
        int maxHeat = 0;
        
        if (useAgg) {
            QSqlQuery heatQuery(sqlDb);
            heatQuery.prepare(
                "SELECT heat_region, SUM(count) as cnt "
                "FROM agg_heatmap_stats WHERE time_bucket >= ? AND time_bucket <= ? "
                "GROUP BY heat_region");
            heatQuery.addBindValue(startBucket);
            heatQuery.addBindValue(endBucket);
            heatQuery.exec();
            
            while (heatQuery.next()) {
                int region = heatQuery.value(0).toInt();
                int cnt = heatQuery.value(1).toInt();
                heatData[region] = cnt;
                maxHeat = qMax(maxHeat, cnt);
            }
        } else {
            qint64 startMs = start.toMSecsSinceEpoch();
            qint64 endMs = end.toMSecsSinceEpoch();
            
            QSqlQuery heatQuery(sqlDb);
            heatQuery.prepare(
                "SELECT heat_region, COUNT(*) as cnt "
                "FROM operations WHERE time >= ? AND time < ? "
                "AND is_main_window = 1 AND event_type IN ('mouse_click','touch_tap','area_click') "
                "GROUP BY heat_region");
            heatQuery.addBindValue(startMs);
            heatQuery.addBindValue(endMs);
            heatQuery.exec();
            
            while (heatQuery.next()) {
                int region = heatQuery.value(0).toInt();
                int cnt = heatQuery.value(1).toInt();
                heatData[region] = cnt;
                maxHeat = qMax(maxHeat, cnt);
            }
        }
        heatmapWidget_->setData(heatData, maxHeat);
        
        // ========== 对话框分析表格 + 散点图 ==========
        {
            auto* analyzer = BehaviorAnalytics::analyzer();
            if (analyzer) {
                auto dialogResult = analyzer->analyzeDialog(start, end);
                QVariantList dialogs = dialogResult.data["dialogs"].toList();
                
                // 按打开次数降序排
                std::sort(dialogs.begin(), dialogs.end(), [](const QVariant& a, const QVariant& b) {
                    return a.toMap()["open_count"].toInt() > b.toMap()["open_count"].toInt();
                });
                
                dialogTable_->setRowCount(dialogs.size());
                auto formatMs = [](int ms) -> QString {
                    if (ms <= 0) return "-";
                    if (ms < 1000) return QString("%1ms").arg(ms);
                    double sec = ms / 1000.0;
                    if (sec < 60) return QString("%1s").arg(sec, 0, 'f', 1);
                    int m = static_cast<int>(sec / 60);
                    double s = sec - m * 60;
                    if (m < 60) return QString("%1m%2s").arg(m).arg(s, 0, 'f', 0);
                    int h = m / 60;
                    int rm = m % 60;
                    return QString("%1h%2m").arg(h).arg(rm);
                };
                
                for (int i = 0; i < dialogs.size(); ++i) {
                    QVariantMap m = dialogs[i].toMap();
                    dialogTable_->setItem(i, 0, new QTableWidgetItem(m["class"].toString()));
                    dialogTable_->setItem(i, 1, new QTableWidgetItem(QString::number(m["open_count"].toInt())));
                    dialogTable_->setItem(i, 2, new QTableWidgetItem(formatMs(m["avg_duration_ms"].toInt())));
                    dialogTable_->setItem(i, 3, new QTableWidgetItem(formatMs(m["median_duration_ms"].toInt())));
                    dialogTable_->setItem(i, 4, new QTableWidgetItem(formatMs(m["min_duration_ms"].toInt())));
                    dialogTable_->setItem(i, 5, new QTableWidgetItem(formatMs(m["max_duration_ms"].toInt())));
                    dialogTable_->setItem(i, 6, new QTableWidgetItem(m["instant_close_rate"].toString()));
                }
                
                // 散点图：X=打开次数, Y=平均时长(秒)
                dialogScatter_->detachItems();
                QVector<double> sx, sy;
                QStringList sLabels;
                double maxX = 0, maxY = 0;
                for (const auto& item : dialogs) {
                    QVariantMap m = item.toMap();
                    int cnt = m["open_count"].toInt();
                    double avgSec = m["avg_duration_ms"].toDouble() / 1000.0;
                    if (cnt <= 0) continue;
                    sx << cnt;
                    sy << avgSec;
                    QString label = m["class"].toString();
                    if (label.length() > 15) label = label.left(12) + "...";
                    sLabels << label;
                    maxX = qMax(maxX, (double)cnt);
                    maxY = qMax(maxY, avgSec);
                }
                
                if (!sx.isEmpty()) {
                    auto* curve = new QwtPlotCurve("Dialog");
                    curve->setStyle(QwtPlotCurve::Dots);
                    // 设置点的大小
                    QwtSymbol* sym = new QwtSymbol(QwtSymbol::Ellipse, 
                        QBrush(QColor("#3B82F6")), QPen(QColor("#1D4ED8"), 2), QSize(14, 14));
                    curve->setSymbol(sym);
                    curve->setSamples(sx.data(), sy.data(), sx.size());
                    curve->attach(dialogScatter_);
                    
                    // 添加标签
                    for (int i = 0; i < sLabels.size(); ++i) {
                        QwtText label(sLabels[i].length() > 15 ? sLabels[i].left(12) + "..." : sLabels[i]);
                        label.setFont(QFont("Microsoft YaHei", 8));
                        label.setColor(QColor("#E2E8F0"));
                        auto* marker = new QwtPlotMarker();
                        marker->setValue(sx[i] + maxX * 0.01, sy[i]);
                        marker->setLabel(label);
                        marker->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                        marker->attach(dialogScatter_);
                    }
                    
                    dialogScatter_->setAxisScale(QwtPlot::xBottom, 0, maxX * 1.15, 0);
                    dialogScatter_->setAxisScale(QwtPlot::yLeft, 0, maxY * 1.15, 0);
                    dialogScatter_->replot();
                }
            }
        }
    }
    
private:
    QDateEdit* startDate_ = nullptr;
    QDateEdit* endDate_ = nullptr;
    QTabWidget* tabWidget_ = nullptr;
    QwtPlot* operationChart_ = nullptr;
    QwtPlot* moduleChart_ = nullptr;
    PieChartWidget* inputChart_ = nullptr;
    QwtPlot* timeChart_ = nullptr;
    HeatmapWidget* heatmapWidget_ = nullptr;
    QTableWidget* dialogTable_ = nullptr;
    QwtPlot* dialogScatter_ = nullptr;
    QList<QLabel*> statCards_;
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("行为采集 SDK Demo");
    setObjectName("MainWindow");
    resize(1920, 1080);

    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();

    auto* actRefresh = new QAction("刷新", this);
    actRefresh->setShortcut(QKeySequence("F5"));
    connect(actRefresh, &QAction::triggered, this, &MainWindow::onRefreshChart);
    addAction(actRefresh);
    
    // 连接实时采集日志信号
    connect(&ui_shared::behavior::BehaviorAnalytics::instance(), 
            &ui_shared::behavior::BehaviorAnalytics::operationRecorded,
            this, &MainWindow::appendLog);
    
    appendLog("程序启动，采集已激活");
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction("导出数据...", this, &MainWindow::onExport, QKeySequence("Ctrl+E"));
    fileMenu->addSeparator();
    fileMenu->addAction("退出", this, &QMainWindow::close, QKeySequence("Ctrl+Q"));
    
    auto* viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("刷新图表", this, &MainWindow::onRefreshChart, QKeySequence("F5"));
    viewMenu->addAction("分析结果...", this, [this]{
        AnalysisDialog dlg(this);
        dlg.exec();
    });
    
    auto* settingsMenu = menuBar()->addMenu("设置(&S)");
    settingsMenu->addAction("系统设置...", this, &MainWindow::onSettings);
    
    auto* helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("关于", this, []{
        QMessageBox::about(nullptr, "关于", "行为采集 SDK Demo v2.0\n\n用于采集和分析用户行为数据");
    });
}

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    
    auto* btnStart = new QAction("开始测量", this);
    btnStart->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(btnStart, &QAction::triggered, this, &MainWindow::onMeasureStart);
    toolbar->addAction(btnStart);
    
    auto* btnStop = new QAction("停止测量", this);
    btnStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    connect(btnStop, &QAction::triggered, this, &MainWindow::onMeasureStop);
    toolbar->addAction(btnStop);
    
    toolbar->addSeparator();
    
    auto* btnRefresh = new QAction("刷新", this);
    btnRefresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connect(btnRefresh, &QAction::triggered, this, &MainWindow::onRefreshChart);
    toolbar->addAction(btnRefresh);
    
    toolbar->addSeparator();
    
    auto* btnSettings = new QAction("设置", this);
    btnSettings->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(btnSettings, &QAction::triggered, this, &MainWindow::onSettings);
    toolbar->addAction(btnSettings);
}

void MainWindow::setupCentralWidget() {
    auto* central = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(central);
    
    // 左侧：图表 + 日志
    auto* leftWidget = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    
    // 图表区域
    auto* tabWidget = new QTabWidget(this);
    
    // 频率图表（柱状图）
    auto* freqPlot = new QwtPlot();
    freqPlot->setTitle("操作频率分析");
    freqPlot->setAxisTitle(QwtPlot::xBottom, "操作类型");
    freqPlot->setAxisTitle(QwtPlot::yLeft, "次数");
    freqPlot->insertLegend(new QwtLegend());
    frequencyChart_ = freqPlot;
    tabWidget->addTab(freqPlot, "操作频率");
    
    // 时段分布图表（折线图）
    auto* timePlot = new QwtPlot();
    timePlot->setTitle("时段分布");
    timePlot->setAxisTitle(QwtPlot::xBottom, "小时");
    timePlot->setAxisTitle(QwtPlot::yLeft, "操作数");
    timePlot->insertLegend(new QwtLegend());
    timeChart_ = timePlot;
    tabWidget->addTab(timePlot, "时段分布");
    
    // 输入方式（饼图）
    auto* inputPie = new PieChartWidget();
    inputChart_ = inputPie;
    tabWidget->addTab(inputPie, "输入方式");
    
    // 热力图
    auto* heatmapWidget = new HeatmapWidget();
    heatmapChart_ = heatmapWidget;
    tabWidget->addTab(heatmapWidget, "点击热力图");
    
    leftLayout->addWidget(tabWidget, 3);
    
    // 日志区域
    auto* logGroup = new QGroupBox("采集日志", this);
    auto* logLayout = new QVBoxLayout(logGroup);
    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(500);
    logView_->setStyleSheet("font-family: Consolas, monospace; font-size: 12px;");
    logLayout->addWidget(logView_);
    leftLayout->addWidget(logGroup, 1);
    
    mainLayout->addWidget(leftWidget, 3);
    
    // 右侧：系统按钮面板
    setupRightPanel();
    mainLayout->addWidget(rightPanel_, 1);
    
    setCentralWidget(central);
    updateCharts();
}

void MainWindow::setupRightPanel() {
    rightPanel_ = new QGroupBox("系统操作", this);
    auto* lay = new QVBoxLayout(rightPanel_);
    
    auto* statusGroup = new QGroupBox("当前状态", rightPanel_);
    auto* statusLay = new QVBoxLayout(statusGroup);
    auto* statusLabel = new QLabel("就绪", statusGroup);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-weight: bold; color: green;");
    statusLay->addWidget(statusLabel);
    lay->addWidget(statusGroup);
    
    lay->addWidget(new QLabel("操作:", rightPanel_));
    
    auto* btnSystem = new QPushButton("系统操作", rightPanel_);
    btnSystem->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(btnSystem, &QPushButton::clicked, this, []{
        SystemButtonsDialog dlg;
        dlg.exec();
    });
    lay->addWidget(btnSystem);
    
    auto* btnReport = new QPushButton("生成报告", rightPanel_);
    btnReport->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    connect(btnReport, &QPushButton::clicked, this, &MainWindow::printReport);
    lay->addWidget(btnReport);
    
    auto* btnExport = new QPushButton("导出数据", rightPanel_);
    btnExport->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::onExport);
    lay->addWidget(btnExport);
    
    auto* btnSettings = new QPushButton("系统设置", rightPanel_);
    btnSettings->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::onSettings);
    lay->addWidget(btnSettings);
    
    auto* btnAggregation = new QPushButton("立即聚合", rightPanel_);
    btnAggregation->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connect(btnAggregation, &QPushButton::clicked, this, &MainWindow::onAggregation);
    lay->addWidget(btnAggregation);
    
    lay->addStretch();
    
    auto* statsGroup = new QGroupBox("统计信息", rightPanel_);
    auto* statsLay = new QVBoxLayout(statsGroup);
    statsLay->addWidget(new QLabel("总操作数: 0", statsGroup));
    statsLay->addWidget(new QLabel("会话时长: 0:00", statsGroup));
    statsLay->addWidget(new QLabel("采集状态: 启用", statsGroup));
    lay->addWidget(statsGroup);
}

void MainWindow::setupStatusBar() {
    auto* status = statusBar();
    
    statusLabel_ = new QLabel("就绪", this);
    status->addWidget(statusLabel_, 1);
    
    status->addWidget(new QLabel(" | ", this));
    
    sessionLabel_ = new QLabel("会话: --", this);
    status->addWidget(sessionLabel_);
    
    status->addWidget(new QLabel(" | ", this));
    
    auto* timeLabel = new QLabel(this);
    status->addWidget(timeLabel);
    
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [timeLabel]{
        timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    });
    timer->start(1000);
    timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
}

void MainWindow::appendLog(const QString& msg) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    logView_->appendPlainText(QString("[%1] %2").arg(timestamp).arg(msg));
}

void MainWindow::updateCharts() {
    auto* analyzer = BehaviorAnalytics::analyzer();
    if (!analyzer) {
        appendLog("分析器未就绪");
        return;
    }
    
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-7);
    
    // 频率图表
    auto* freqPlot = qobject_cast<QwtPlot*>(frequencyChart_);
    freqPlot->detachItems();
    
    auto freqResult = analyzer->analyzeFrequency(start, end);
    QVector<double> y;
    QStringList actionNames;
    double maxVal = 0;
    
    // 数据格式是 top_actions 列表
    QVariantList topActions = freqResult.data["top_actions"].toList();
    for (const auto& item : topActions) {
        QVariantMap m = item.toMap();
        actionNames << m["action"].toString();
        double v = m["count"].toDouble();
        y << v;
        maxVal = qMax(maxVal, v);
    }
    
    auto* freqChart = new QwtPlotBarChart("操作频率");
    freqChart->setSamples(y);
    freqChart->attach(freqPlot);
    
    // 设置 X 轴范围和标签
    if (!y.isEmpty()) {
        freqPlot->setAxisScale(QwtPlot::xBottom, 0, y.size() - 1, 1);
    }
    freqPlot->setAxisScale(QwtPlot::yLeft, 0, maxVal > 0 ? maxVal * 1.2 : 100);
    freqPlot->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(actionNames));
    freqPlot->replot();
    
    // 时段分布图表
    auto* timePlot = qobject_cast<QwtPlot*>(timeChart_);
    timePlot->detachItems();
    
    auto timeResult = analyzer->analyzeTime(start, end);
    QVector<double> tx, ty;
    maxVal = 0;
    
    // 数据格式是 by_hour 列表
    QVariantList byHour = timeResult.data["by_hour"].toList();
    for (const auto& item : byHour) {
        QVariantMap m = item.toMap();
        tx << m["bucket"].toDouble();
        double v = m["count"].toDouble();
        ty << v;
        maxVal = qMax(maxVal, v);
    }
    
    auto* timeCurve = new QwtPlotCurve("时段分布");
    timeCurve->setSamples(tx.data(), ty.data(), tx.size());
    timeCurve->setPen(QPen(QColor(66, 133, 244), 2));
    timeCurve->attach(timePlot);
    timePlot->setAxisScale(QwtPlot::xBottom, 0, 23);
    timePlot->setAxisScale(QwtPlot::yLeft, 0, maxVal > 0 ? maxVal * 1.2 : 100);
    timePlot->replot();
    
    // 输入方式图表（饼图）
    auto* inputPie = qobject_cast<PieChartWidget*>(inputChart_);
    
    auto inputResult = analyzer->analyzeInput(start, end);
    y.clear();
    
    // 数据格式是 mouse/touch/keyboard/scroll/knob 五个对象
    QStringList types = {"mouse", "touch", "keyboard", "scroll", "knob"};
    QStringList typeLabels = {"鼠标", "触摸", "键盘", "滚动", "旋钮"};
    for (const QString& type : types) {
        QVariantMap m = inputResult.data[type].toMap();
        double v = m["count"].toDouble();
        y << v;
    }
    
    inputPie->setData(typeLabels, y);
    
    // 热力图
    auto* heatmapWidget = qobject_cast<HeatmapWidget*>(heatmapChart_);
    auto heatmapResult = analyzer->analyzeHeatmap(start, end);
    
    QMap<int, int> regionCounts;
    int maxHeatCount = 0;
    QVariantList regions = heatmapResult.data["regions"].toList();
    for (const auto& item : regions) {
        QVariantMap m = item.toMap();
        int region = m["region"].toInt();
        int count = m["count"].toInt();
        regionCounts[region] = count;
        maxHeatCount = qMax(maxHeatCount, count);
    }
    heatmapWidget->setData(regionCounts, maxHeatCount);
    
    appendLog(QString("图表已更新 - 频率:%1 时段:%2 输入:%3 热力图:%4")
        .arg(topActions.size()).arg(byHour.size()).arg(y.size()).arg(regions.size()));
}

void MainWindow::onMeasureStart() {
    measuring_ = true;
    statusLabel_->setText("测量中...");
    statusLabel_->setStyleSheet("color: blue;");
    statusBar()->showMessage("开始测量", 2000);
    appendLog("开始测量");
}

void MainWindow::onMeasureStop() {
    measuring_ = false;
    statusLabel_->setText("已停止");
    statusLabel_->setStyleSheet("color: orange;");
    statusBar()->showMessage("测量已停止", 2000);
    appendLog("停止测量");
}

void MainWindow::onSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        statusBar()->showMessage("设置已保存", 2000);
        appendLog("设置已更新");
    }
}

void MainWindow::onExport() {
    QString path = QFileDialog::getSaveFileName(this, "导出数据", 
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".json",
        "JSON文件 (*.json);;所有文件 (*)");
    if (!path.isEmpty()) {
        QMessageBox::information(this, "导出", "数据已导出到:\n" + path);
        appendLog(QString("数据导出到: %1").arg(path));
    }
}

void MainWindow::onRefreshChart() {
    statusLabel_->setText("刷新中...");
    updateCharts();
    statusLabel_->setText("就绪");
    statusBar()->showMessage("图表已刷新", 2000);
}

void MainWindow::onAggregation() {
    appendLog("开始全量聚合...");
    statusLabel_->setText("聚合中...");
    
    // 获取数据库连接
    auto& db = ui_shared::behavior::Database::instance();
    if (!db.isOpen()) {
        appendLog("错误: 数据库未打开");
        statusLabel_->setText("聚合失败");
        return;
    }
    
    QSqlDatabase sqlDb = db.connection();
    
    // 清除旧的错误数据
    QSqlQuery cleanQuery(sqlDb);
    cleanQuery.exec("DELETE FROM agg_heatmap_stats");
    cleanQuery.exec("DELETE FROM agg_operation_stats");
    cleanQuery.exec("DELETE FROM agg_module_stats");
    cleanQuery.exec("DELETE FROM agg_input_stats");
    cleanQuery.exec("DELETE FROM agg_dialog_stats");
    cleanQuery.exec("DELETE FROM agg_time_distribution");
    appendLog("已清除所有聚合数据");
    
    // 测试时间转换
    QSqlQuery testTime(sqlDb);
    testTime.exec("SELECT datetime(1719142800,'unixepoch'), datetime(1719142800,'unixepoch','localtime')");
    if (testTime.next()) {
        appendLog(QString("UTC: %1, Local: %2").arg(testTime.value(0).toString()).arg(testTime.value(1).toString()));
    }
    
    // 查询 operations 表的记录数
    QSqlQuery countQuery(sqlDb);
    countQuery.exec("SELECT COUNT(*) FROM operations");
    int opsCount = 0;
    if (countQuery.next()) {
        opsCount = countQuery.value(0).toInt();
    }
    appendLog(QString("operations 表记录数: %1").arg(opsCount));
    
    // 检查数据字段
    QSqlQuery fieldQuery(sqlDb);
    fieldQuery.exec("SELECT action_name, control_name, control_class, event_type FROM operations LIMIT 10");
    appendLog("前10条记录的字段值:");
    while (fieldQuery.next()) {
        QString actionName = fieldQuery.value(0).toString();
        QString controlName = fieldQuery.value(1).toString();
        QString controlClass = fieldQuery.value(2).toString();
        QString eventType = fieldQuery.value(3).toString();
        appendLog(QString("  action=%1, control=%2, class=%3, type=%4")
            .arg(actionName.isEmpty() ? "(空)" : actionName)
            .arg(controlName.isEmpty() ? "(空)" : controlName)
            .arg(controlClass.isEmpty() ? "(空)" : controlClass)
            .arg(eventType));
    }
    
    // 检查事件类型分布
    QSqlQuery eventQuery(sqlDb);
    eventQuery.exec("SELECT event_type, COUNT(*) as cnt FROM operations GROUP BY event_type");
    appendLog("事件类型分布:");
    while (eventQuery.next()) {
        appendLog(QString("  %1: %2 条").arg(eventQuery.value(0).toString()).arg(eventQuery.value(1).toInt()));
    }
    
    if (opsCount == 0) {
        appendLog("错误: operations 表为空，请先采集数据");
        statusLabel_->setText("聚合失败");
        return;
    }
    
    // 查询时间范围
    QSqlQuery rangeQuery(sqlDb);
    rangeQuery.exec("SELECT MIN(time), MAX(time) FROM operations");
    if (rangeQuery.next()) {
        qint64 minTime = rangeQuery.value(0).toLongLong();
        qint64 maxTime = rangeQuery.value(1).toLongLong();
        QDateTime minDt = QDateTime::fromMSecsSinceEpoch(minTime);
        QDateTime maxDt = QDateTime::fromMSecsSinceEpoch(maxTime);
        appendLog(QString("数据时间范围: %1 至 %2")
            .arg(minDt.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(maxDt.toString("yyyy-MM-dd HH:mm:ss")));
    }
    
    // 执行聚合 - 修改 SQL，使用 control_class 作为后备
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-30);
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    
    appendLog(QString("聚合时间范围: %1 至 %2")
        .arg(start.toString("yyyy-MM-dd HH:mm:ss"))
        .arg(end.toString("yyyy-MM-dd HH:mm:ss")));
    
    // 使用 COALESCE(action_name, control_name, control_class, event_type) 确保不为空
    // 注意：使用 'localtime' 转换为本地时间，天粒度
    
    // 开始事务，避免数据库锁定
    QSqlQuery transQuery(sqlDb);
    transQuery.exec("BEGIN IMMEDIATE TRANSACTION");
    
    QString insertSql = QString(
        "INSERT OR REPLACE INTO agg_operation_stats (time_bucket,granularity,action_key,action_type,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), NULLIF(control_class,''), event_type), "
        "event_type, COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "GROUP BY 1, 3, 4")
        .arg("%Y-%m-%d")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery insertQuery(sqlDb);
    if (!insertQuery.exec(insertSql)) {
        appendLog(QString("插入错误: %1").arg(insertQuery.lastError().text()));
    } else {
        appendLog("agg_operation_stats 插入成功");
    }
    
    // 聚合其他表
    QString moduleSql = QString(
        "INSERT OR REPLACE INTO agg_module_stats (time_bucket,granularity,module_class,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "COALESCE(NULLIF(window_class,''), 'unknown'), COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "GROUP BY 1, 3")
        .arg("%Y-%m-%d")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery moduleQuery(sqlDb);
    if (!moduleQuery.exec(moduleSql)) {
        appendLog(QString("module 错误: %1").arg(moduleQuery.lastError().text()));
    } else {
        appendLog("agg_module_stats 插入成功");
    }
    
    QString inputSql = QString(
        "INSERT OR REPLACE INTO agg_input_stats (time_bucket,granularity,input_method,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "input_method, COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "GROUP BY 1, 3")
        .arg("%Y-%m-%d")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery inputQuery(sqlDb);
    if (!inputQuery.exec(inputSql)) {
        appendLog(QString("input 错误: %1").arg(inputQuery.lastError().text()));
    } else {
        appendLog("agg_input_stats 插入成功");
    }
    
    // 热力图聚合
    QString heatmapSql = QString(
        "INSERT OR REPLACE INTO agg_heatmap_stats (time_bucket,granularity,heat_region,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "heat_region, COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "AND is_main_window = 1 AND event_type IN ('mouse_click','touch_tap','area_click') "
        "GROUP BY 1, 3")
        .arg("%Y-%m-%d")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery heatmapQuery(sqlDb);
    if (!heatmapQuery.exec(heatmapSql)) {
        appendLog(QString("heatmap 错误: %1").arg(heatmapQuery.lastError().text()));
    } else {
        appendLog("agg_heatmap_stats 插入成功");
    }
    
    // 对话框聚合 — 标识优先级与 dialog_analyzer 一致
    QString dialogSql = QString(
        "INSERT OR REPLACE INTO agg_dialog_stats (time_bucket,granularity,dialog_class,open_count,total_duration,avg_duration) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "COALESCE(NULLIF(window_title,''), NULLIF(control_name,''), window_class), "
        "SUM(CASE WHEN event_type='dialog_open' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,0) ELSE 0 END), "
        "AVG(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,0) END) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "AND event_type IN ('dialog_open','dialog_close') "
        "GROUP BY 1, 3")
        .arg("%Y-%m-%d")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery dialogQuery(sqlDb);
    if (!dialogQuery.exec(dialogSql)) {
        appendLog(QString("dialog 错误: %1").arg(dialogQuery.lastError().text()));
    } else {
        appendLog("agg_dialog_stats 插入成功");
    }
    
    // 时间分布聚合（天粒度）
    QString timeSql = QString(
        "INSERT OR REPLACE INTO agg_time_distribution (date,hour,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), "
        "0, COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 GROUP BY 1")
        .arg("%Y-%m-%d")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery timeQuery(sqlDb);
    if (!timeQuery.exec(timeSql)) {
        appendLog(QString("time 错误: %1").arg(timeQuery.lastError().text()));
    } else {
        appendLog("agg_time_distribution 插入成功");
    }
    
    // 提交事务
    transQuery.exec("COMMIT");
    appendLog("事务已提交");
    
    // 检查聚合结果
    QStringList tableNames = {
        "agg_operation_stats", "agg_module_stats", "agg_input_stats",
        "agg_heatmap_stats", "agg_dialog_stats", "agg_time_distribution"
    };
    
    for (const QString& table : tableNames) {
        QSqlQuery tq(sqlDb);
        tq.exec(QString("SELECT COUNT(*) FROM %1").arg(table));
        if (tq.next()) {
            int cnt = tq.value(0).toInt();
            appendLog(QString("%1: %2 条").arg(table).arg(cnt));
        }
    }
    
    // 检查 time_bucket 实际值
    QSqlQuery tbQuery(sqlDb);
    tbQuery.exec("SELECT DISTINCT time_bucket FROM agg_operation_stats ORDER BY time_bucket DESC LIMIT 10");
    appendLog("agg_operation_stats time_bucket 实际值:");
    while (tbQuery.next()) {
        appendLog(QString("  '%1'").arg(tbQuery.value(0).toString()));
    }
    
    tbQuery.exec("SELECT DISTINCT time_bucket FROM agg_heatmap_stats LIMIT 5");
    appendLog("agg_heatmap_stats time_bucket 实际值:");
    while (tbQuery.next()) {
        appendLog(QString("  '%1'").arg(tbQuery.value(0).toString()));
    }
    
    // 检查今天的数据
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    tbQuery.prepare("SELECT COUNT(*) FROM agg_operation_stats WHERE time_bucket = ?");
    tbQuery.addBindValue(today);
    tbQuery.exec();
    if (tbQuery.next()) {
        int todayCount = tbQuery.value(0).toInt();
        appendLog(QString("今天(%1)的聚合数据: %2 条").arg(today).arg(todayCount));
    }
    
    appendLog("聚合完成");
    statusLabel_->setText("就绪");
    statusBar()->showMessage("聚合完成", 3000);
    
    // 刷新图表
    updateCharts();
}

void MainWindow::printReport() {
    auto* a = BehaviorAnalytics::analyzer();
    if (!a) { 
        QMessageBox::warning(this, "错误", "分析器未就绪");
        return; 
    }
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-7);

    QString report;
    report += "==== 操作频率 ====\n";
    auto freqData = a->analyzeFrequency(start, end).data;
    for (auto it = freqData.begin(); it != freqData.end(); ++it)
        report += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());
    report += "\n==== 模块热度 ====\n";
    auto modData = a->analyzeModule(start, end).data;
    for (auto it = modData.begin(); it != modData.end(); ++it)
        report += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());
    report += "\n==== 输入方式 ====\n";
    auto inputData = a->analyzeInput(start, end).data;
    for (auto it = inputData.begin(); it != inputData.end(); ++it)
        report += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());
    report += "\n==== 时段分布 ====\n";
    auto timeData = a->analyzeTime(start, end).data;
    for (auto it = timeData.begin(); it != timeData.end(); ++it)
        report += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());

    QMessageBox::information(this, "分析报告", report);
    appendLog("生成分析报告");
}

#include "mainwindow.moc"
