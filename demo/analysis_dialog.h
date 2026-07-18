#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QLabel>

class DateField;
class OperationsTab;
class ModulesTab;
class InputTab;
class TimeTab;
class HeatmapTab;
class DialogTab;
class DistanceTab;

class AnalysisDialog : public QDialog {
    Q_OBJECT
public:
    explicit AnalysisDialog(QWidget* parent = nullptr);

private slots:
    void onAnalyze();

private:
    DateField* startDate_ = nullptr;
    DateField* endDate_ = nullptr;
    QTabWidget* tabWidget_ = nullptr;
    QList<QLabel*> statCards_;

    OperationsTab* operationsTab_ = nullptr;
    ModulesTab* modulesTab_ = nullptr;
    InputTab* inputTab_ = nullptr;
    TimeTab* timeTab_ = nullptr;
    HeatmapTab* heatmapTab_ = nullptr;
    DialogTab* dialogTab_ = nullptr;
    DistanceTab* distanceTab_ = nullptr;
};
