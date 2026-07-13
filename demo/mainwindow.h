#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPlainTextEdit>
#include <QGroupBox>

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

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupRightPanel();
    void setupStatusBar();
    void appendLog(const QString& msg);
    void updateCharts();

    QWidget* frequencyChart_ = nullptr;
    QWidget* timeChart_ = nullptr;
    QWidget* inputChart_ = nullptr;
    QWidget* heatmapChart_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;
    QGroupBox* rightPanel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* sessionLabel_ = nullptr;
    bool measuring_ = true;
};
