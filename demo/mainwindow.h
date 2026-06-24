#pragma once

#include <QMainWindow>

class QLabel;
class QGroupBox;
class QPlainTextEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onMeasureStart();
    void onMeasureStop();
    void onSettings();
    void onExport();
    void onRefreshChart();
    void onAggregation();
    void printReport();
    void appendLog(const QString& msg);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    void setupRightPanel();
    void updateCharts();
    
    QWidget* m_frequencyChart = nullptr;
    QWidget* m_timeChart = nullptr;
    QWidget* m_inputChart = nullptr;
    QWidget* m_heatmapChart = nullptr;
    
    QPlainTextEdit* m_logView = nullptr;
    
    QLabel* m_statusLabel = nullptr;
    QLabel* m_sessionLabel = nullptr;
    
    QGroupBox* m_rightPanel = nullptr;
    
    bool m_measuring = false;
};
