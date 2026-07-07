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
    
    QWidget* frequencyChart_ = nullptr;
    QWidget* timeChart_ = nullptr;
    QWidget* inputChart_ = nullptr;
    QWidget* heatmapChart_ = nullptr;
    
    QPlainTextEdit* logView_ = nullptr;
    
    QLabel* statusLabel_ = nullptr;
    QLabel* sessionLabel_ = nullptr;
    
    QGroupBox* rightPanel_ = nullptr;
    
    bool measuring_ = false;
};
