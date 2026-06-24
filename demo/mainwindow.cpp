#include "mainwindow.h"
#include "core/BehaviorAnalytics.h"
#include "core/Types.h"
#include "analyzer/BehaviorAnalyzer.h"
#include "storage/Database.h"
#include "storage/Aggregator.h"

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
#include <QDateTimeEdit>
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
#include <QPainter>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

// Qwt
#include <qwt_plot.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_histogram.h>
#include <qwt_legend.h>
#include <qwt_column_symbol.h>
#include <qwt_series_data.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_draw.h>
#include <qwt_text.h>

using namespace ui_shared::behavior;

// 自定义 X 轴刻度绘制，显示操作名称
class ActionScaleDraw : public QwtScaleDraw {
public:
    explicit ActionScaleDraw(const QStringList& labels) : m_labels(labels) {
        enableComponent(QwtScaleDraw::Ticks, false);
        enableComponent(QwtScaleDraw::Backbone, false);
    }
    
    QwtText label(double value) const override {
        int idx = static_cast<int>(value + 0.5);  // 四舍五入
        if (idx >= 0 && idx < m_labels.size()) {
            QString name = m_labels[idx];
            // 截断过长的名称
            if (name.length() > 8) {
                name = name.left(6) + "..";
            }
            return QwtText(name, QwtText::PlainText);
        }
        return QwtText();  // 返回空文本，不显示标签
    }
    
private:
    QStringList m_labels;
};

// 简单的热力图绘制控件
class HeatmapWidget : public QWidget {
    Q_OBJECT
public:
    explicit HeatmapWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(400, 400);
    }
    
    void setData(const QMap<int, int>& regions, int maxCount) {
        m_regions = regions;
        m_maxCount = qMax(1, maxCount);
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
        p.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop, "主窗口点击热力图");
        
        // 绘制网格
        for (int row = 0; row < 10; ++row) {
            for (int col = 0; col < 10; ++col) {
                int region = row * 10 + col;
                int count = m_regions.value(region, 0);
                
                // 计算颜色（从白到红）
                double intensity = (m_maxCount > 0) ? (double)count / m_maxCount : 0;
                QColor color = QColor::fromHsvF(0, intensity, 1 - intensity * 0.3);
                
                QRect cell(r.left() + col * cellW, r.top() + row * cellH, cellW, cellH);
                p.fillRect(cell, color);
                p.setPen(Qt::gray);
                p.drawRect(cell);
                
                // 显示数值
                if (count > 0) {
                    p.setPen(Qt::black);
                    p.drawText(cell, Qt::AlignCenter, QString::number(count));
                }
            }
        }
        
        // 绘制坐标轴标签
        p.setPen(Qt::black);
        for (int i = 0; i < 10; ++i) {
            // X轴（列）
            p.drawText(r.left() + i * cellW + cellW/2 - 5, r.bottom() + 15, QString::number(i));
            // Y轴（行）
            p.drawText(r.left() - 20, r.top() + i * cellH + cellH/2 + 5, QString::number(i));
        }
        p.drawText(r.left() - 35, r.top() + r.height()/2, "行");
        p.drawText(r.left() + r.width()/2 - 10, r.bottom() + 30, "列");
    }
    
private:
    QMap<int, int> m_regions;
    int m_maxCount = 1;
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
            QMessageBox::information(nullptr, "清空", "数据清空功能");
        });
    }
};

// 分析结果对话框
class AnalysisDialog : public QDialog {
    Q_OBJECT
public:
    explicit AnalysisDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("行为分析结果");
        setMinimumSize(1000, 750);
        
        // 应用数据仪表盘样式
        setStyleSheet(R"(
            QDialog {
                background-color: #F8FAFC;
            }
            QGroupBox {
                font-weight: 600;
                font-size: 13px;
                color: #1E3A8A;
                border: 1px solid #DBEAFE;
                border-radius: 6px;
                margin-top: 12px;
                padding-top: 8px;
                background-color: white;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 8px;
                background-color: white;
            }
            QLabel {
                color: #1E3A8A;
                font-size: 13px;
            }
            QPushButton {
                background-color: #E9EEF6;
                color: #1E3A8A;
                border: 1px solid #DBEAFE;
                border-radius: 4px;
                padding: 6px 12px;
                font-size: 13px;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: #DBEAFE;
                border-color: #3B82F6;
            }
            QPushButton:pressed {
                background-color: #1E40AF;
                color: white;
            }
            QPushButton:default {
                background-color: #1E40AF;
                color: white;
                border-color: #1E40AF;
            }
            QPushButton:default:hover {
                background-color: #3B82F6;
            }
            QDateTimeEdit {
                background-color: white;
                border: 1px solid #DBEAFE;
                border-radius: 4px;
                padding: 4px 8px;
                color: #1E3A8A;
            }
            QDateTimeEdit:focus {
                border-color: #3B82F6;
            }
            QTabWidget::pane {
                border: 1px solid #DBEAFE;
                background-color: white;
                border-radius: 6px;
            }
            QTabBar::tab {
                background-color: #E9EEF6;
                color: #1E3A8A;
                border: 1px solid #DBEAFE;
                padding: 8px 16px;
                margin-right: 2px;
                border-top-left-radius: 4px;
                border-top-right-radius: 4px;
            }
            QTabBar::tab:selected {
                background-color: white;
                border-bottom-color: white;
                font-weight: 600;
            }
            QTabBar::tab:hover:!selected {
                background-color: #DBEAFE;
            }
            QTableWidget {
                background-color: white;
                border: none;
                gridline-color: #E9EEF6;
                font-size: 13px;
            }
            QTableWidget::item {
                padding: 6px;
                color: #1E3A8A;
            }
            QTableWidget::item:selected {
                background-color: #DBEAFE;
                color: #1E3A8A;
            }
            QTableWidget::item:hover {
                background-color: #F8FAFC;
            }
            QHeaderView::section {
                background-color: #F8FAFC;
                color: #1E3A8A;
                font-weight: 600;
                font-size: 12px;
                padding: 8px;
                border: none;
                border-bottom: 2px solid #DBEAFE;
            }
            QScrollBar:vertical {
                background-color: #F8FAFC;
                width: 10px;
                border-radius: 5px;
            }
            QScrollBar::handle:vertical {
                background-color: #DBEAFE;
                border-radius: 5px;
                min-height: 20px;
            }
            QScrollBar::handle:vertical:hover {
                background-color: #3B82F6;
            }
        )");
        
        auto* lay = new QVBoxLayout(this);
        lay->setSpacing(12);
        lay->setContentsMargins(16, 16, 16, 16);
        
        // 时间范围选择
        auto* timeGroup = new QGroupBox("统计区间", this);
        auto* timeLay = new QHBoxLayout(timeGroup);
        timeLay->setSpacing(8);
        
        timeLay->addWidget(new QLabel("开始时间:", this));
        m_startTime = new QDateTimeEdit(this);
        m_startTime->setCalendarPopup(true);
        m_startTime->setDateTime(QDateTime::currentDateTime().addDays(-1));
        m_startTime->setDisplayFormat("yyyy-MM-dd HH:mm");
        timeLay->addWidget(m_startTime);
        
        timeLay->addSpacing(12);
        timeLay->addWidget(new QLabel("结束时间:", this));
        m_endTime = new QDateTimeEdit(this);
        m_endTime->setCalendarPopup(true);
        m_endTime->setDateTime(QDateTime::currentDateTime());
        m_endTime->setDisplayFormat("yyyy-MM-dd HH:mm");
        timeLay->addWidget(m_endTime);
        
        timeLay->addSpacing(16);
        
        // 快捷按钮
        auto* btnLastHour = new QPushButton("最近1小时", this);
        auto* btnLastDay = new QPushButton("最近1天", this);
        auto* btnLastWeek = new QPushButton("最近7天", this);
        timeLay->addWidget(btnLastHour);
        timeLay->addWidget(btnLastDay);
        timeLay->addWidget(btnLastWeek);
        
        timeLay->addSpacing(8);
        
        auto* btnAnalyze = new QPushButton("分析", this);
        btnAnalyze->setDefault(true);
        btnAnalyze->setStyleSheet(R"(
            QPushButton {
                background-color: #D97706;
                color: white;
                border: none;
                padding: 8px 20px;
                font-weight: 600;
            }
            QPushButton:hover {
                background-color: #B45309;
            }
            QPushButton:pressed {
                background-color: #92400E;
            }
        )");
        timeLay->addWidget(btnAnalyze);
        
        timeLay->addStretch();
        
        lay->addWidget(timeGroup);
        
        // 结果显示区域（使用 tab）
        m_tabWidget = new QTabWidget(this);
        
        // 操作频率 tab - 柱状图
        m_operationChart = new QwtPlot(this);
        m_operationChart->setCanvasBackground(Qt::white);
        m_operationChart->setAxisTitle(QwtPlot::yLeft, "次数");
        m_operationChart->setAxisTitle(QwtPlot::xBottom, "操作");
        m_tabWidget->addTab(m_operationChart, "操作频率");
        
        // 模块统计 tab - 柱状图
        m_moduleChart = new QwtPlot(this);
        m_moduleChart->setCanvasBackground(Qt::white);
        m_moduleChart->setAxisTitle(QwtPlot::yLeft, "次数");
        m_moduleChart->setAxisTitle(QwtPlot::xBottom, "模块");
        m_tabWidget->addTab(m_moduleChart, "模块统计");
        
        // 输入方式 tab - 柱状图
        m_inputChart = new QwtPlot(this);
        m_inputChart->setCanvasBackground(Qt::white);
        m_inputChart->setAxisTitle(QwtPlot::yLeft, "次数");
        m_inputChart->setAxisTitle(QwtPlot::xBottom, "输入方式");
        m_tabWidget->addTab(m_inputChart, "输入方式");
        
        // 时间分布 tab - 折线图
        m_timeChart = new QwtPlot(this);
        m_timeChart->setCanvasBackground(Qt::white);
        m_timeChart->setAxisTitle(QwtPlot::yLeft, "次数");
        m_timeChart->setAxisTitle(QwtPlot::xBottom, "时间");
        m_tabWidget->addTab(m_timeChart, "时间分布");
        
        // 热力图 tab
        m_heatmapWidget = new HeatmapWidget(this);
        m_tabWidget->addTab(m_heatmapWidget, "点击热力图");
        
        lay->addWidget(m_tabWidget);
        
        // 底部按钮
        auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
        lay->addWidget(btns);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        // 连接信号
        connect(btnLastHour, &QPushButton::clicked, this, [this]{
            m_endTime->setDateTime(QDateTime::currentDateTime());
            m_startTime->setDateTime(QDateTime::currentDateTime().addSecs(-3600));
        });
        connect(btnLastDay, &QPushButton::clicked, this, [this]{
            m_endTime->setDateTime(QDateTime::currentDateTime());
            m_startTime->setDateTime(QDateTime::currentDateTime().addDays(-1));
        });
        connect(btnLastWeek, &QPushButton::clicked, this, [this]{
            m_endTime->setDateTime(QDateTime::currentDateTime());
            m_startTime->setDateTime(QDateTime::currentDateTime().addDays(-7));
        });
        connect(btnAnalyze, &QPushButton::clicked, this, &AnalysisDialog::onAnalyze);
        
        // 弹窗打开时自动执行一次统计
        QTimer::singleShot(100, this, &AnalysisDialog::onAnalyze);
    }
    
private slots:
    void onAnalyze() {
        QDateTime start = m_startTime->dateTime();
        QDateTime end = m_endTime->dateTime();
        
        auto& db = ui_shared::behavior::Database::instance();
        QSqlDatabase sqlDb = db.connection();
        if (!sqlDb.isOpen()) {
            QMessageBox::warning(this, "错误", "数据库未打开");
            return;
        }
        
        // 格式化时间桶范围
        QString startBucket = start.toString("yyyy-MM-dd HH:00");
        QString endBucket = end.toString("yyyy-MM-dd HH:00");
        
        // 检查聚合表是否有数据
        QSqlQuery checkQuery(sqlDb);
        checkQuery.exec("SELECT COUNT(*) FROM agg_operation_stats");
        int aggCount = 0;
        if (checkQuery.next()) aggCount = checkQuery.value(0).toInt();
        
        bool useAgg = (aggCount > 0);
        int total = 0;
        
        // ========== 操作频率图表 ==========
        m_operationChart->detachItems();
        QVector<double> opData;
        QStringList opLabels;
        
        if (useAgg) {
            QSqlQuery opQuery(sqlDb);
            opQuery.prepare(
                "SELECT action_key, SUM(count) as cnt "
                "FROM agg_operation_stats "
                "WHERE time_bucket >= ? AND time_bucket < ? "
                "GROUP BY action_key ORDER BY cnt DESC LIMIT 15");
            opQuery.addBindValue(startBucket);
            opQuery.addBindValue(endBucket);
            opQuery.exec();
            
            QSqlQuery totalQuery(sqlDb);
            totalQuery.prepare("SELECT SUM(count) FROM agg_operation_stats WHERE time_bucket >= ? AND time_bucket < ?");
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
            auto* opChart = new QwtPlotBarChart("操作频率");
            opChart->setSamples(opData);
            opChart->attach(m_operationChart);
            m_operationChart->setAxisScale(QwtPlot::xBottom, 0, opData.size() - 1, 1);
            m_operationChart->setAxisScale(QwtPlot::yLeft, 0, *std::max_element(opData.begin(), opData.end()) * 1.2);
            m_operationChart->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(opLabels));
            m_operationChart->replot();
        }
        
        // ========== 模块统计图表 ==========
        m_moduleChart->detachItems();
        QVector<double> modData;
        QStringList modLabels;
        
        if (useAgg) {
            QSqlQuery modQuery(sqlDb);
            modQuery.prepare(
                "SELECT module_class, SUM(count) as cnt "
                "FROM agg_module_stats WHERE time_bucket >= ? AND time_bucket < ? "
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
            auto* modChart = new QwtPlotBarChart("模块统计");
            modChart->setSamples(modData);
            modChart->attach(m_moduleChart);
            m_moduleChart->setAxisScale(QwtPlot::xBottom, 0, modData.size() - 1, 1);
            m_moduleChart->setAxisScale(QwtPlot::yLeft, 0, *std::max_element(modData.begin(), modData.end()) * 1.2);
            m_moduleChart->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(modLabels));
            m_moduleChart->replot();
        }
        
        // ========== 输入方式图表 ==========
        m_inputChart->detachItems();
        QVector<double> inputData;
        QStringList inputLabels;
        
        if (useAgg) {
            QSqlQuery inputQuery(sqlDb);
            inputQuery.prepare(
                "SELECT input_method, SUM(count) as cnt "
                "FROM agg_input_stats WHERE time_bucket >= ? AND time_bucket < ? "
                "GROUP BY input_method ORDER BY cnt DESC");
            inputQuery.addBindValue(startBucket);
            inputQuery.addBindValue(endBucket);
            inputQuery.exec();
            
            while (inputQuery.next()) {
                QString method = inputQuery.value(0).toString();
                if (method == "mouse") method = "鼠标";
                else if (method == "touch") method = "触屏";
                else if (method == "keyboard") method = "键盘";
                inputLabels << method;
                inputData << inputQuery.value(1).toDouble();
            }
        } else {
            qint64 startMs = start.toMSecsSinceEpoch();
            qint64 endMs = end.toMSecsSinceEpoch();
            
            QSqlQuery inputQuery(sqlDb);
            inputQuery.prepare(
                "SELECT input_method, COUNT(*) as cnt "
                "FROM operations WHERE time >= ? AND time < ? GROUP BY input_method ORDER BY cnt DESC");
            inputQuery.addBindValue(startMs);
            inputQuery.addBindValue(endMs);
            inputQuery.exec();
            
            while (inputQuery.next()) {
                QString method = inputQuery.value(0).toString();
                if (method == "mouse") method = "鼠标";
                else if (method == "touch") method = "触屏";
                else if (method == "keyboard") method = "键盘";
                inputLabels << method;
                inputData << inputQuery.value(1).toDouble();
            }
        }
        
        if (!inputData.isEmpty()) {
            auto* inputChart = new QwtPlotBarChart("输入方式");
            inputChart->setSamples(inputData);
            inputChart->attach(m_inputChart);
            m_inputChart->setAxisScale(QwtPlot::xBottom, 0, inputData.size() - 1, 1);
            m_inputChart->setAxisScale(QwtPlot::yLeft, 0, *std::max_element(inputData.begin(), inputData.end()) * 1.2);
            m_inputChart->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(inputLabels));
            m_inputChart->replot();
        }
        
        // ========== 时间分布图表（折线图） ==========
        m_timeChart->detachItems();
        QVector<double> timeData;
        QStringList timeLabels;
        
        if (useAgg) {
            QSqlQuery timeQuery(sqlDb);
            timeQuery.prepare(
                "SELECT date || ' ' || hour || ':00' as time, SUM(count) as cnt "
                "FROM agg_time_distribution WHERE date >= ? AND date <= ? "
                "GROUP BY date, hour ORDER BY date, hour");
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
                "SELECT strftime('%Y-%m-%d %H:00', datetime(time/1000,'unixepoch','localtime')) as time, COUNT(*) as cnt "
                "FROM operations WHERE time >= ? AND time < ? GROUP BY time ORDER BY time");
            timeQuery.addBindValue(startMs);
            timeQuery.addBindValue(endMs);
            timeQuery.exec();
            
            while (timeQuery.next()) {
                timeLabels << timeQuery.value(0).toString();
                timeData << timeQuery.value(1).toDouble();
            }
        }
        
        if (!timeData.isEmpty()) {
            auto* timeCurve = new QwtPlotCurve("时间分布");
            QVector<double> x(timeData.size());
            for (int i = 0; i < timeData.size(); ++i) x[i] = i;
            timeCurve->setSamples(x.data(), timeData.data(), timeData.size());
            timeCurve->setPen(QPen(QColor("#1E40AF"), 2));
            timeCurve->attach(m_timeChart);
            m_timeChart->setAxisScale(QwtPlot::xBottom, 0, timeData.size() - 1, qMax(1, timeData.size() / 10));
            m_timeChart->setAxisScale(QwtPlot::yLeft, 0, *std::max_element(timeData.begin(), timeData.end()) * 1.2);
            m_timeChart->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(timeLabels));
            m_timeChart->replot();
        }
        
        // ========== 热力图 ==========
        QMap<int, int> heatData;
        int maxHeat = 0;
        
        if (useAgg) {
            QSqlQuery heatQuery(sqlDb);
            heatQuery.prepare(
                "SELECT heat_region, SUM(count) as cnt "
                "FROM agg_heatmap_stats WHERE time_bucket >= ? AND time_bucket < ? "
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
        m_heatmapWidget->setData(heatData, maxHeat);
    }
    
private:
    QDateTimeEdit* m_startTime = nullptr;
    QDateTimeEdit* m_endTime = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QwtPlot* m_operationChart = nullptr;
    QwtPlot* m_moduleChart = nullptr;
    QwtPlot* m_inputChart = nullptr;
    QwtPlot* m_timeChart = nullptr;
    HeatmapWidget* m_heatmapWidget = nullptr;
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
    m_frequencyChart = freqPlot;
    tabWidget->addTab(freqPlot, "操作频率");
    
    // 时段分布图表（折线图）
    auto* timePlot = new QwtPlot();
    timePlot->setTitle("时段分布");
    timePlot->setAxisTitle(QwtPlot::xBottom, "小时");
    timePlot->setAxisTitle(QwtPlot::yLeft, "操作数");
    timePlot->insertLegend(new QwtLegend());
    m_timeChart = timePlot;
    tabWidget->addTab(timePlot, "时段分布");
    
    // 输入方式（柱状图）
    auto* inputPlot = new QwtPlot();
    inputPlot->setTitle("输入方式分布");
    inputPlot->setAxisTitle(QwtPlot::xBottom, "类型");
    inputPlot->setAxisTitle(QwtPlot::yLeft, "次数");
    inputPlot->insertLegend(new QwtLegend());
    m_inputChart = inputPlot;
    tabWidget->addTab(inputPlot, "输入方式");
    
    // 热力图
    auto* heatmapWidget = new HeatmapWidget();
    m_heatmapChart = heatmapWidget;
    tabWidget->addTab(heatmapWidget, "点击热力图");
    
    leftLayout->addWidget(tabWidget, 3);
    
    // 日志区域
    auto* logGroup = new QGroupBox("采集日志", this);
    auto* logLayout = new QVBoxLayout(logGroup);
    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(500);
    m_logView->setStyleSheet("font-family: Consolas, monospace; font-size: 12px;");
    logLayout->addWidget(m_logView);
    leftLayout->addWidget(logGroup, 1);
    
    mainLayout->addWidget(leftWidget, 3);
    
    // 右侧：系统按钮面板
    setupRightPanel();
    mainLayout->addWidget(m_rightPanel, 1);
    
    setCentralWidget(central);
    updateCharts();
}

void MainWindow::setupRightPanel() {
    m_rightPanel = new QGroupBox("系统操作", this);
    auto* lay = new QVBoxLayout(m_rightPanel);
    
    auto* statusGroup = new QGroupBox("当前状态", m_rightPanel);
    auto* statusLay = new QVBoxLayout(statusGroup);
    auto* statusLabel = new QLabel("就绪", statusGroup);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-weight: bold; color: green;");
    statusLay->addWidget(statusLabel);
    lay->addWidget(statusGroup);
    
    lay->addWidget(new QLabel("操作:", m_rightPanel));
    
    auto* btnSystem = new QPushButton("系统操作", m_rightPanel);
    btnSystem->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(btnSystem, &QPushButton::clicked, this, []{
        SystemButtonsDialog dlg;
        dlg.exec();
    });
    lay->addWidget(btnSystem);
    
    auto* btnReport = new QPushButton("生成报告", m_rightPanel);
    btnReport->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    connect(btnReport, &QPushButton::clicked, this, &MainWindow::printReport);
    lay->addWidget(btnReport);
    
    auto* btnExport = new QPushButton("导出数据", m_rightPanel);
    btnExport->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::onExport);
    lay->addWidget(btnExport);
    
    auto* btnSettings = new QPushButton("系统设置", m_rightPanel);
    btnSettings->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::onSettings);
    lay->addWidget(btnSettings);
    
    auto* btnAggregation = new QPushButton("立即聚合", m_rightPanel);
    btnAggregation->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connect(btnAggregation, &QPushButton::clicked, this, &MainWindow::onAggregation);
    lay->addWidget(btnAggregation);
    
    lay->addStretch();
    
    auto* statsGroup = new QGroupBox("统计信息", m_rightPanel);
    auto* statsLay = new QVBoxLayout(statsGroup);
    statsLay->addWidget(new QLabel("总操作数: 0", statsGroup));
    statsLay->addWidget(new QLabel("会话时长: 0:00", statsGroup));
    statsLay->addWidget(new QLabel("采集状态: 启用", statsGroup));
    lay->addWidget(statsGroup);
}

void MainWindow::setupStatusBar() {
    auto* status = statusBar();
    
    m_statusLabel = new QLabel("就绪", this);
    status->addWidget(m_statusLabel, 1);
    
    status->addWidget(new QLabel(" | ", this));
    
    m_sessionLabel = new QLabel("会话: --", this);
    status->addWidget(m_sessionLabel);
    
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
    m_logView->appendPlainText(QString("[%1] %2").arg(timestamp).arg(msg));
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
    auto* freqPlot = qobject_cast<QwtPlot*>(m_frequencyChart);
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
    auto* timePlot = qobject_cast<QwtPlot*>(m_timeChart);
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
    
    // 输入方式图表
    auto* inputPlot = qobject_cast<QwtPlot*>(m_inputChart);
    inputPlot->detachItems();
    
    auto inputResult = analyzer->analyzeInput(start, end);
    y.clear();
    maxVal = 0;
    
    // 数据格式是 mouse/touch/keyboard 三个对象
    QStringList types = {"mouse", "touch", "keyboard"};
    QStringList typeLabels = {"鼠标", "触摸", "键盘"};
    for (const QString& type : types) {
        QVariantMap m = inputResult.data[type].toMap();
        double v = m["count"].toDouble();
        y << v;
        maxVal = qMax(maxVal, v);
    }
    
    auto* inputChart = new QwtPlotBarChart("输入方式");
    inputChart->setSamples(y);
    inputChart->attach(inputPlot);
    inputPlot->setAxisScale(QwtPlot::xBottom, 0, 2, 1);  // 只显示 0, 1, 2 三个刻度
    inputPlot->setAxisScale(QwtPlot::yLeft, 0, maxVal > 0 ? maxVal * 1.2 : 100);
    inputPlot->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(typeLabels));
    inputPlot->replot();
    
    // 热力图
    auto* heatmapWidget = qobject_cast<HeatmapWidget*>(m_heatmapChart);
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
    m_measuring = true;
    m_statusLabel->setText("测量中...");
    m_statusLabel->setStyleSheet("color: blue;");
    statusBar()->showMessage("开始测量", 2000);
    appendLog("开始测量");
}

void MainWindow::onMeasureStop() {
    m_measuring = false;
    m_statusLabel->setText("已停止");
    m_statusLabel->setStyleSheet("color: orange;");
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
    m_statusLabel->setText("刷新中...");
    updateCharts();
    m_statusLabel->setText("就绪");
    statusBar()->showMessage("图表已刷新", 2000);
}

void MainWindow::onAggregation() {
    appendLog("开始全量聚合...");
    m_statusLabel->setText("聚合中...");
    
    // 获取数据库连接
    auto& db = ui_shared::behavior::Database::instance();
    if (!db.isOpen()) {
        appendLog("错误: 数据库未打开");
        m_statusLabel->setText("聚合失败");
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
        m_statusLabel->setText("聚合失败");
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
    
    // 使用 COALESCE(action_name, control_name, control_class, event_type) 确保不为空
    // 注意：使用 'localtime' 转换为本地时间
    QString insertSql = QString(
        "INSERT OR REPLACE INTO agg_operation_stats (time_bucket,granularity,action_key,action_type,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'hour', "
        "COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), NULLIF(control_class,''), event_type), "
        "event_type, COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "GROUP BY 1, 3, 4")
        .arg("%Y-%m-%d %H:00")
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
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'hour', "
        "COALESCE(NULLIF(window_class,''), 'unknown'), COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "GROUP BY 1, 3")
        .arg("%Y-%m-%d %H:00")
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
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'hour', "
        "input_method, COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "GROUP BY 1, 3")
        .arg("%Y-%m-%d %H:00")
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
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'hour', "
        "heat_region, COUNT(*) "
        "FROM operations WHERE time >= %2 AND time < %3 "
        "AND is_main_window = 1 AND event_type IN ('mouse_click','touch_tap','area_click') "
        "GROUP BY 1, 3")
        .arg("%Y-%m-%d %H:00")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery heatmapQuery(sqlDb);
    if (!heatmapQuery.exec(heatmapSql)) {
        appendLog(QString("heatmap 错误: %1").arg(heatmapQuery.lastError().text()));
    } else {
        appendLog("agg_heatmap_stats 插入成功");
    }
    
    // 对话框聚合
    QString dialogSql = QString(
        "INSERT OR REPLACE INTO agg_dialog_stats (time_bucket,granularity,dialog_class,open_count,total_duration,avg_duration) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), 'hour', "
        "window_class, COUNT(*), SUM(COALESCE(duration,0)), AVG(COALESCE(duration,0)) "
        "FROM operations WHERE time >= %2 AND time < %3 AND event_type = 'dialog_close' "
        "GROUP BY 1, 3")
        .arg("%Y-%m-%d %H:00")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery dialogQuery(sqlDb);
    if (!dialogQuery.exec(dialogSql)) {
        appendLog(QString("dialog 错误: %1").arg(dialogQuery.lastError().text()));
    } else {
        appendLog("agg_dialog_stats 插入成功");
    }
    
    // 时间分布聚合
    QString timeSql = QString(
        "INSERT OR REPLACE INTO agg_time_distribution (date,hour,count) "
        "SELECT strftime('%1', datetime(time/1000,'unixepoch','localtime')), "
        "CAST(strftime('%2', datetime(time/1000,'unixepoch','localtime')) AS INTEGER), COUNT(*) "
        "FROM operations WHERE time >= %3 AND time < %4 GROUP BY 1, 2")
        .arg("%Y-%m-%d")
        .arg("%H")
        .arg(startMs)
        .arg(endMs);
    
    QSqlQuery timeQuery(sqlDb);
    if (!timeQuery.exec(timeSql)) {
        appendLog(QString("time 错误: %1").arg(timeQuery.lastError().text()));
    } else {
        appendLog("agg_time_distribution 插入成功");
    }
    
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
    tbQuery.exec("SELECT DISTINCT time_bucket FROM agg_heatmap_stats LIMIT 5");
    appendLog("time_bucket 实际值:");
    while (tbQuery.next()) {
        appendLog(QString("  '%1'").arg(tbQuery.value(0).toString()));
    }
    
    appendLog("聚合完成");
    m_statusLabel->setText("就绪");
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
