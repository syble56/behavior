#include "mainwindow.h"
#include "analysis_dialog.h"
#include "settings_dialog.h"
#include "chart_widgets.h"
#include "core/behavior_analytics.h"
#include "analyzer/behavior_analyzer.h"
#include "core/types.h"
#include "storage/database.h"
#include "storage/aggregator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QAction>
#include <QKeySequence>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QMenuBar>
#include <QTabWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

#include <qwt_plot.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_curve.h>
#include <qwt_legend.h>

using namespace ui_shared::behavior;

// ============ Constructor ============

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Behavior SDK Demo");
    setObjectName("MainWindow");
    resize(1920, 1080);

    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();

    auto* actRefresh = new QAction("Refresh", this);
    actRefresh->setShortcut(QKeySequence("F5"));
    connect(actRefresh, &QAction::triggered, this, &MainWindow::onRefreshChart);
    addAction(actRefresh);

    connect(&BehaviorAnalytics::instance(),
            &BehaviorAnalytics::operationRecorded,
            this, &MainWindow::appendLog);

    appendLog("Started, collection active");
}

// ============ Menu Bar ============

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Export...", this, &MainWindow::onExport, QKeySequence("Ctrl+E"));
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QMainWindow::close, QKeySequence("Ctrl+Q"));

    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Refresh", this, &MainWindow::onRefreshChart, QKeySequence("F5"));
    viewMenu->addAction("Analysis...", this, [this]{
        AnalysisDialog dlg(this);
        dlg.exec();
    });

    auto* settingsMenu = menuBar()->addMenu("&Settings");
    settingsMenu->addAction("Settings...", this, &MainWindow::onSettings);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About", this, []{
        QMessageBox::about(nullptr, "About", "Behavior SDK Demo v2.0");
    });
}

// ============ Toolbar ============

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    auto* btnStart = new QAction("Start", this);
    btnStart->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(btnStart, &QAction::triggered, this, &MainWindow::onMeasureStart);
    toolbar->addAction(btnStart);

    auto* btnStop = new QAction("Stop", this);
    btnStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    connect(btnStop, &QAction::triggered, this, &MainWindow::onMeasureStop);
    toolbar->addAction(btnStop);

    toolbar->addSeparator();

    auto* btnRefresh = new QAction("Refresh", this);
    btnRefresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connect(btnRefresh, &QAction::triggered, this, &MainWindow::onRefreshChart);
    toolbar->addAction(btnRefresh);

    toolbar->addSeparator();

    auto* btnSettings = new QAction("Settings", this);
    btnSettings->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(btnSettings, &QAction::triggered, this, &MainWindow::onSettings);
    toolbar->addAction(btnSettings);
}

// ============ Central Widget ============

void MainWindow::setupCentralWidget() {
    auto* central = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(central);

    auto* leftWidget = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWidget);

    auto* tabWidget = new QTabWidget(this);

    auto* freqPlot = new QwtPlot();
    freqPlot->setTitle("Operation Frequency");
    freqPlot->setAxisTitle(QwtPlot::xBottom, "Action");
    freqPlot->setAxisTitle(QwtPlot::yLeft, "Count");
    freqPlot->insertLegend(new QwtLegend());
    frequencyChart_ = freqPlot;
    tabWidget->addTab(freqPlot, "Frequency");

    auto* timePlot = new QwtPlot();
    timePlot->setTitle("Hourly Distribution");
    timePlot->setAxisTitle(QwtPlot::xBottom, "Hour");
    timePlot->setAxisTitle(QwtPlot::yLeft, "Count");
    timePlot->insertLegend(new QwtLegend());
    timeChart_ = timePlot;
    tabWidget->addTab(timePlot, "Hourly");

    auto* inputPie = new PieChartWidget();
    inputChart_ = inputPie;
    tabWidget->addTab(inputPie, "Input");

    auto* heatmapWidget = new HeatmapWidget();
    heatmapChart_ = heatmapWidget;
    tabWidget->addTab(heatmapWidget, "Heatmap");

    leftLayout->addWidget(tabWidget, 3);

    auto* logGroup = new QGroupBox("Log", this);
    auto* logLayout = new QVBoxLayout(logGroup);
    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(500);
    logView_->setStyleSheet("font-family: Consolas, monospace; font-size: 12px;");
    logLayout->addWidget(logView_);
    leftLayout->addWidget(logGroup, 1);

    mainLayout->addWidget(leftWidget, 3);

    setupRightPanel();
    mainLayout->addWidget(rightPanel_, 1);

    setCentralWidget(central);
    updateCharts();
}

// ============ Right Panel ============

void MainWindow::setupRightPanel() {
    rightPanel_ = new QGroupBox("Actions", this);
    auto* lay = new QVBoxLayout(rightPanel_);

    auto* statusGroup = new QGroupBox("Status", rightPanel_);
    auto* statusLay = new QVBoxLayout(statusGroup);
    statusLabel_ = new QLabel("Ready", statusGroup);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setStyleSheet("font-weight: bold; color: green;");
    statusLay->addWidget(statusLabel_);
    lay->addWidget(statusGroup);

    auto* btnSystem = new QPushButton("System Actions", rightPanel_);
    btnSystem->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(btnSystem, &QPushButton::clicked, this, []{
        SystemButtonsDialog dlg;
        dlg.exec();
    });
    lay->addWidget(btnSystem);

    auto* btnReport = new QPushButton("Generate Report", rightPanel_);
    btnReport->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    connect(btnReport, &QPushButton::clicked, this, &MainWindow::printReport);
    lay->addWidget(btnReport);

    auto* btnExport = new QPushButton("Export Data", rightPanel_);
    btnExport->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::onExport);
    lay->addWidget(btnExport);

    auto* btnSettings = new QPushButton("Settings", rightPanel_);
    btnSettings->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::onSettings);
    lay->addWidget(btnSettings);

    auto* btnAggregation = new QPushButton("Aggregate Now", rightPanel_);
    btnAggregation->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connect(btnAggregation, &QPushButton::clicked, this, &MainWindow::onAggregation);
    lay->addWidget(btnAggregation);

    lay->addStretch();

    auto* statsGroup = new QGroupBox("Stats", rightPanel_);
    auto* statsLay = new QVBoxLayout(statsGroup);
    statsLay->addWidget(new QLabel("Total ops: 0", statsGroup));
    statsLay->addWidget(new QLabel("Session: 0:00", statsGroup));
    statsLay->addWidget(new QLabel("Collection: enabled", statsGroup));
    lay->addWidget(statsGroup);
}

// ============ Status Bar ============

void MainWindow::setupStatusBar() {
    auto* status = statusBar();

    statusLabel_ = new QLabel("Ready", this);
    status->addWidget(statusLabel_, 1);
    status->addWidget(new QLabel(" | ", this));

    sessionLabel_ = new QLabel("Session: --", this);
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

// ============ Log ============

void MainWindow::appendLog(const QString& msg) {
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    logView_->appendPlainText(QString("[%1] %2").arg(ts).arg(msg));
}

// ============ Update Charts ============

void MainWindow::updateCharts() {
    auto* analyzer = BehaviorAnalytics::analyzer();
    if (!analyzer) { appendLog("Analyzer not ready"); return; }

    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-7);

    // Frequency
    auto* freqPlot = qobject_cast<QwtPlot*>(frequencyChart_);
    freqPlot->detachItems();
    auto freqResult = analyzer->analyzeFrequency(start, end);
    QVector<double> y;
    QStringList actionNames;
    double maxVal = 0;
    for (const auto& item : freqResult.data["top_actions"].toList()) {
        QVariantMap m = item.toMap();
        actionNames << m["action"].toString();
        double v = m["count"].toDouble();
        y << v; maxVal = qMax(maxVal, v);
    }
    auto* freqChart = new QwtPlotBarChart("Frequency");
    freqChart->setSamples(y);
    freqChart->attach(freqPlot);
    if (!y.isEmpty()) freqPlot->setAxisScale(QwtPlot::xBottom, 0, y.size() - 1, 1);
    freqPlot->setAxisScale(QwtPlot::yLeft, 0, maxVal > 0 ? maxVal * 1.2 : 100);
    freqPlot->setAxisScaleDraw(QwtPlot::xBottom, new ActionScaleDraw(actionNames));
    freqPlot->replot();

    // Hourly
    auto* timePlot = qobject_cast<QwtPlot*>(timeChart_);
    timePlot->detachItems();
    auto timeResult = analyzer->analyzeTime(start, end);
    QVector<double> tx, ty;
    maxVal = 0;
    for (const auto& item : timeResult.data["by_hour"].toList()) {
        QVariantMap m = item.toMap();
        tx << m["bucket"].toDouble();
        double v = m["count"].toDouble();
        ty << v; maxVal = qMax(maxVal, v);
    }
    auto* timeCurve = new QwtPlotCurve("Hourly");
    timeCurve->setSamples(tx.data(), ty.data(), tx.size());
    timeCurve->setPen(QPen(QColor(66, 133, 244), 2));
    timeCurve->attach(timePlot);
    timePlot->setAxisScale(QwtPlot::xBottom, 0, 23);
    timePlot->setAxisScale(QwtPlot::yLeft, 0, maxVal > 0 ? maxVal * 1.2 : 100);
    timePlot->replot();

    // Input pie
    auto* inputPie = qobject_cast<PieChartWidget*>(inputChart_);
    auto inputResult = analyzer->analyzeInput(start, end);
    QVector<double> inputVals;
    QStringList inputLabels = {"Mouse", "Touch", "Keyboard", "Scroll", "Knob"};
    for (const QString& type : {"mouse", "touch", "keyboard", "scroll", "knob"}) {
        inputVals << inputResult.data[type].toMap()["count"].toDouble();
    }
    inputPie->setData(inputLabels, inputVals);

    // Heatmap
    auto* heatmapWidget = qobject_cast<HeatmapWidget*>(heatmapChart_);
    auto heatmapResult = analyzer->analyzeHeatmap(start, end);
    QMap<int, int> regionCounts;
    int maxHeat = 0;
    for (const auto& item : heatmapResult.data["regions"].toList()) {
        QVariantMap m = item.toMap();
        int region = m["region"].toInt();
        int count = m["count"].toInt();
        regionCounts[region] = count;
        maxHeat = qMax(maxHeat, count);
    }
    heatmapWidget->setData(regionCounts, maxHeat);

    appendLog(QString("Charts updated - freq:%1 hourly:%2 input:%3 heat:%4")
        .arg(y.size()).arg(ty.size()).arg(inputVals.size()).arg(regionCounts.size()));
}

// ============ Slots ============

void MainWindow::onMeasureStart() {
    measuring_ = true;
    statusLabel_->setText("Measuring...");
    statusLabel_->setStyleSheet("color: blue;");
    statusBar()->showMessage("Started", 2000);
    appendLog("Measurement started");
}

void MainWindow::onMeasureStop() {
    measuring_ = false;
    statusLabel_->setText("Stopped");
    statusLabel_->setStyleSheet("color: orange;");
    statusBar()->showMessage("Stopped", 2000);
    appendLog("Measurement stopped");
}

void MainWindow::onSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        statusBar()->showMessage("Settings saved", 2000);
        appendLog("Settings updated");
    }
}

void MainWindow::onExport() {
    QString path = QFileDialog::getSaveFileName(this, "Export",
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".json",
        "JSON (*.json);;All (*)");
    if (!path.isEmpty()) {
        QMessageBox::information(this, "Export", "Data exported to:\n" + path);
        appendLog(QString("Exported to: %1").arg(path));
    }
}

void MainWindow::onRefreshChart() {
    statusLabel_->setText("Refreshing...");
    updateCharts();
    statusLabel_->setText("Ready");
    statusBar()->showMessage("Charts refreshed", 2000);
}

void MainWindow::onAggregation() {
    appendLog("Starting aggregation...");
    statusLabel_->setText("Aggregating...");

    auto& db = Database::instance();
    if (!db.isOpen()) { appendLog("Error: DB not open"); statusLabel_->setText("Failed"); return; }

    QSqlDatabase sqlDb = db.connection();

    // Clean old data
    QSqlQuery q(sqlDb);
    for (const auto& t : {"agg_heatmap_stats", "agg_operation_stats", "agg_module_stats",
                          "agg_input_stats", "agg_dialog_stats", "agg_time_distribution"})
        q.exec(QString("DELETE FROM %1").arg(t));
    appendLog("Cleared aggregation tables");

    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-30);
    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();

    q.exec("BEGIN IMMEDIATE TRANSACTION");

    // operation_stats
    q.exec(QString(
        "INSERT OR REPLACE INTO agg_operation_stats (time_bucket,granularity,action_key,action_type,count) "
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "COALESCE(NULLIF(action_name,''), NULLIF(control_name,''), NULLIF(control_class,''), event_type), "
        "event_type, COUNT(*) FROM operations WHERE time >= %1 AND time < %2 GROUP BY 1, 3, 4")
        .arg(startMs).arg(endMs));

    // module_stats
    q.exec(QString(
        "INSERT OR REPLACE INTO agg_module_stats (time_bucket,granularity,module_class,count) "
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "COALESCE(NULLIF(window_class,''), 'unknown'), COUNT(*) "
        "FROM operations WHERE time >= %1 AND time < %2 GROUP BY 1, 3")
        .arg(startMs).arg(endMs));

    // input_stats
    q.exec(QString(
        "INSERT OR REPLACE INTO agg_input_stats (time_bucket,granularity,input_method,count) "
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "input_method, COUNT(*) FROM operations WHERE time >= %1 AND time < %2 GROUP BY 1, 3")
        .arg(startMs).arg(endMs));

    // heatmap_stats
    q.exec(QString(
        "INSERT OR REPLACE INTO agg_heatmap_stats (time_bucket,granularity,heat_region,count) "
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "heat_region, COUNT(*) FROM operations WHERE time >= %1 AND time < %2 "
        "AND is_main_window = 1 AND event_type IN ('mouse_click','touch_tap','area_click') GROUP BY 1, 3")
        .arg(startMs).arg(endMs));

    // dialog_stats
    q.exec(QString(
        "INSERT OR REPLACE INTO agg_dialog_stats (time_bucket,granularity,dialog_class,open_count,total_duration,avg_duration) "
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')), 'day', "
        "COALESCE(NULLIF(window_title,''), NULLIF(control_name,''), window_class), "
        "SUM(CASE WHEN event_type='dialog_open' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,0) ELSE 0 END), "
        "AVG(CASE WHEN event_type='dialog_close' THEN COALESCE(duration,0) END) "
        "FROM operations WHERE time >= %1 AND time < %2 AND event_type IN ('dialog_open','dialog_close') GROUP BY 1, 3")
        .arg(startMs).arg(endMs));

    // time_distribution
    q.exec(QString(
        "INSERT OR REPLACE INTO agg_time_distribution (date,hour,count) "
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')), 0, COUNT(*) "
        "FROM operations WHERE time >= %1 AND time < %2 GROUP BY 1")
        .arg(startMs).arg(endMs));

    q.exec("COMMIT");
    appendLog("Aggregation complete");
    statusLabel_->setText("Ready");
    statusBar()->showMessage("Aggregation done", 3000);
    updateCharts();
}

void MainWindow::printReport() {
    auto* a = BehaviorAnalytics::analyzer();
    if (!a) { QMessageBox::warning(this, "Error", "Analyzer not ready"); return; }

    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-7);

    QString report;
    report += "==== Frequency ====\n";
    for (auto it = a->analyzeFrequency(start, end).data.begin(); it != a->analyzeFrequency(start, end).data.end(); ++it)
        report += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());
    report += "\n==== Module ====\n";
    for (auto it = a->analyzeModule(start, end).data.begin(); it != a->analyzeModule(start, end).data.end(); ++it)
        report += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());
    report += "\n==== Input ====\n";
    for (auto it = a->analyzeInput(start, end).data.begin(); it != a->analyzeInput(start, end).data.end(); ++it)
        report += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());
    report += "\n==== Time ====\n";
    for (auto it = a->analyzeTime(start, end).data.begin(); it != a->analyzeTime(start, end).data.end(); ++it)
        report += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());

    QMessageBox::information(this, "Report", report);
    appendLog("Report generated");
}
