#include "analysis_dialog.h"
#include "date_picker/date_field.h"
#include "operations_tab.h"
#include "modules_tab.h"
#include "input_tab.h"
#include "time_tab.h"
#include "heatmap_tab.h"
#include "dialog_tab.h"
#include "distance_tab.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QTimer>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QDebug>
#include <QComboBox>

using namespace ui_shared::behavior;

namespace {
constexpr int  kDialogMinWidth     = 1100;
constexpr int  kDialogMinHeight    = 800;
constexpr int  kLayoutSpacing      = 12;
constexpr int  kMargin             = 16;
constexpr int  kSpacingSmall       = 8;
constexpr int  kSpacingMid         = 12;
constexpr int  kCardSpacing        = 10;
constexpr int  kCardMinHeight      = 72;
constexpr int  kComboMinWidth      = 120;
constexpr int  kInitDelayMs        = 100;
constexpr int  kPresetToday        = 0;
constexpr int  kPresetWeek         = 1;
constexpr int  kPresetMonth        = 2;
constexpr int  kPresetHalfYear     = 3;
constexpr int  kPresetCustom       = 4;
constexpr int  kPresetWeekDays     = 6;
constexpr int  kPresetMonthDays    = 29;
constexpr int  kPresetHalfYearDays = 179;
constexpr int  kCardAvgDistance    = 2;
constexpr auto kDateFormat         = "yyyy-MM-dd";
}

// ============ Constructor ============

AnalysisDialog::AnalysisDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Behavior Analysis");
    setMinimumSize(kDialogMinWidth, kDialogMinHeight);

    setupStyle();

    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(kLayoutSpacing);
    lay->setContentsMargins(kMargin, kMargin, kMargin, kMargin);

    QComboBox* rangeCombo = nullptr;
    setupDateRange(lay, rangeCombo);
    setupSummaryCards(lay);
    setupTabs(lay);

    auto labels = findChildren<QLabel*>();
    QLabel* startLabel = nullptr;
    QLabel* endLabel = nullptr;
    for (auto* lbl : labels) {
        if (lbl->text() == "Start:") startLabel = lbl;
        if (lbl->text() == "End:")   endLabel = lbl;
    }

    setupSignals(rangeCombo, startLabel, endLabel);

    QTimer::singleShot(kInitDelayMs, this, &AnalysisDialog::onAnalyze);
}

void AnalysisDialog::setupStyle() {
    setStyleSheet(R"(
        QDialog { background-color: #32353A; }
        QGroupBox {
            font-weight: 600; font-size: 13px; color: #E2E8F0;
            border: 1px solid #4A4D52; border-radius: 8px;
            margin-top: 12px; padding-top: 10px; background-color: #3C3F44;
        }
        QGroupBox::title {
            subcontrol-origin: margin; subcontrol-position: top left;
            padding: 2px 10px; background-color: #3C3F44; color: #CBD5E1;
        }
        QLabel { color: #E2E8F0; font-size: 13px; }
        QLabel#statCard {
            background-color: #3C3F44; border: 1px solid #4A4D52;
            border-radius: 8px; padding: 12px 16px; font-size: 13px; color: #94A3B8;
        }
        QPushButton {
            background-color: #3C3F44; color: #E2E8F0; border: 1px solid #4A4D52;
            border-radius: 6px; padding: 7px 14px; font-size: 13px; font-weight: 500;
        }
        QPushButton:hover { background-color: #4A4D52; border-color: #64748B; }
        QPushButton:pressed { background-color: #5A5D62; }
        QPushButton:default { background-color: #2563EB; color: white; border-color: #2563EB; }
        QPushButton:default:hover { background-color: #1D4ED8; }
        QDateTimeEdit {
            background-color: #3C3F44; border: 1px solid #4A4D52; border-radius: 6px;
            padding: 5px 10px; color: #FFFFFF; font-size: 13px;
        }
        QDateTimeEdit:focus { border-color: #3B82F6; }
        QDateTimeEdit::drop-down { border: none; width: 20px; }
        QDateTimeEdit::down-arrow {
            image: none; border-left: 4px solid transparent; border-right: 4px solid transparent;
            border-top: 5px solid #94A3B8; width: 0; height: 0; margin-right: 6px;
        }
        QLabel#dateField {
            background-color: #3C3F44; border: 1px solid #4A4D52; border-radius: 6px;
            padding: 5px 10px; color: #FFFFFF; font-size: 13px;
        }
        QLabel#dateField:hover { border-color: #3B82F6; }
        QCalendarWidget {
            background-color: #32353A; color: #E2E8F0; border: 1px solid #4A4D52;
        }
        QCalendarWidget QToolButton {
            background-color: #3C3F44; color: #E2E8F0; border: none; border-radius: 4px;
            padding: 4px 8px; font-size: 13px; font-weight: 600;
        }
        QCalendarWidget QToolButton:hover { background-color: #4A4D52; }
        QCalendarWidget QMenu { background-color: #3C3F44; color: #E2E8F0; border: 1px solid #4A4D52; }
        QCalendarWidget QMenu::item:selected { background-color: #2563EB; }
        QCalendarWidget QAbstractItemView {
            background-color: #32353A; color: #E2E8F0; selection-background-color: #2563EB;
            selection-color: #FFFFFF; alternate-background-color: #34373C; border: none;
        }
        QCalendarWidget QAbstractItemView:enabled { color: #E2E8F0; }
        QCalendarWidget QAbstractItemView:disabled { color: #4A4D52; }
        QTabWidget::pane {
            border: 1px solid #4A4D52; background-color: #3C3F44; border-radius: 8px; top: -1px;
        }
        QTabBar::tab {
            background-color: #32353A; color: #94A3B8; border: 1px solid #4A4D52;
            border-bottom: none; padding: 9px 18px; margin-right: 3px;
            border-top-left-radius: 6px; border-top-right-radius: 6px;
            font-size: 13px; font-weight: 500;
        }
        QTabBar::tab:selected {
            background-color: #3C3F44; color: #FFFFFF; border-bottom-color: #3C3F44; font-weight: 600;
        }
        QTabBar::tab:hover:!selected { background-color: #4A4D52; color: #E2E8F0; }
        QTableWidget {
            background-color: #3C3F44; alternate-background-color: #34373C;
            border: none; gridline-color: #4A4D52; font-size: 13px; border-radius: 8px;
        }
        QTableWidget::item { padding: 8px 6px; color: #E2E8F0; }
        QTableWidget::item:selected { background-color: #2563EB; color: #FFFFFF; }
        QTableWidget::item:hover { background-color: #4A4D52; }
        QHeaderView::section {
            background-color: #32353A; color: #CBD5E1; font-weight: 600; font-size: 12px;
            padding: 10px 8px; border: none; border-bottom: 2px solid #4A4D52;
        }
        QHeaderView::section:vertical {
            background-color: #32353A; color: #94A3B8; border-right: 1px solid #4A4D52; border-bottom: none;
        }
        QScrollBar:vertical { background-color: #32353A; width: 10px; border-radius: 5px; margin: 0; }
        QScrollBar::handle:vertical { background-color: #64748B; border-radius: 5px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background-color: #94A3B8; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background-color: #32353A; height: 10px; border-radius: 5px; margin: 0; }
        QScrollBar::handle:horizontal { background-color: #64748B; border-radius: 5px; min-width: 30px; }
        QScrollBar::handle:horizontal:hover { background-color: #94A3B8; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    )");
}

void AnalysisDialog::setupDateRange(QVBoxLayout* parentLay, QComboBox*& rangeCombo) {
    auto* timeGroup = new QGroupBox("Date Range", this);
    auto* timeLay = new QHBoxLayout(timeGroup);
    timeLay->setSpacing(kSpacingSmall);

    timeLay->addWidget(new QLabel("Range:", this));
    rangeCombo = new QComboBox(this);
    rangeCombo->setMinimumWidth(kComboMinWidth);
    rangeCombo->addItems({"Today", "Last 1 Week", "Last 1 Month", "Last 6 Months", "Custom"});
    rangeCombo->setCurrentIndex(kPresetWeek);
    timeLay->addWidget(rangeCombo);
    timeLay->addSpacing(kSpacingSmall);

    auto* startLabel = new QLabel("Start:", this);
    startDate_ = new DateField(this);
    startDate_->setDate(QDate::currentDate().addDays(-kPresetWeekDays));
    startDate_->setDisplayFormat(kDateFormat);
    startDate_->setLanguage(DateField::English);

    auto* endLabel = new QLabel("End:", this);
    endDate_ = new DateField(this);
    endDate_->setDate(QDate::currentDate());
    endDate_->setDisplayFormat(kDateFormat);
    endDate_->setLanguage(DateField::English);

    startLabel->hide();
    startDate_->hide();
    endLabel->hide();
    endDate_->hide();

    timeLay->addWidget(startLabel);
    timeLay->addWidget(startDate_);
    timeLay->addSpacing(kSpacingMid);
    timeLay->addWidget(endLabel);
    timeLay->addWidget(endDate_);
    timeLay->addSpacing(kSpacingSmall);

    auto* btnAnalyze = new QPushButton("Analyze", this);
    btnAnalyze->setDefault(true);
    btnAnalyze->setStyleSheet(R"(
        QPushButton { background-color: #2563EB; color: white; border: none; border-radius: 6px; padding: 8px 24px; font-weight: 600; font-size: 13px; }
        QPushButton:hover { background-color: #1D4ED8; }
        QPushButton:pressed { background-color: #1E40AF; }
    )");
    timeLay->addWidget(btnAnalyze);
    timeLay->addStretch();
    parentLay->addWidget(timeGroup);
}

void AnalysisDialog::setupSummaryCards(QVBoxLayout* parentLay) {
    auto* cardLay = new QHBoxLayout();
    cardLay->setSpacing(kCardSpacing);

    auto makeCard = [](const char* title, const char* value) {
        auto* card = new QLabel();
        card->setObjectName("statCard");
        card->setText(QString(
            "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>%1</span></td></tr>"
            "<tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>%2</span></td></tr></table>")
            .arg(title).arg(value));
        card->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        card->setMinimumHeight(kCardMinHeight);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return card;
    };

    statCards_ = {
        makeCard("Total Events", "0"),
        makeCard("Active Days", "0"),
        makeCard("Avg Distance", "0px"),
        makeCard("Dialog Opens", "0")
    };

    for (auto* card : statCards_) cardLay->addWidget(card);
    parentLay->addLayout(cardLay);
}

void AnalysisDialog::setupTabs(QVBoxLayout* parentLay) {
    tabWidget_ = new QTabWidget(this);

    operationsTab_ = new OperationsTab(this);
    tabWidget_->addTab(operationsTab_, "Operations");

    modulesTab_ = new ModulesTab(this);
    tabWidget_->addTab(modulesTab_, "Modules");

    inputTab_ = new InputTab(this);
    tabWidget_->addTab(inputTab_, "Input Methods");

    timeTab_ = new TimeTab(this);
    tabWidget_->addTab(timeTab_, "Time Distribution");

    heatmapTab_ = new HeatmapTab(this);
    tabWidget_->addTab(heatmapTab_, "Click Heatmap");

    dialogTab_ = new DialogTab(this);
    tabWidget_->addTab(dialogTab_, "Dialog Analysis");

    distanceTab_ = new DistanceTab(this);
    tabWidget_->addTab(distanceTab_, "Operation Distance");

    connect(distanceTab_, &DistanceTab::avgDistanceChanged, this,
        [this](int avgPx) {
            statCards_[kCardAvgDistance]->setText(QString(
                "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Avg Distance</span></td></tr>"
                "<tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>%1px</span></td></tr></table>")
                .arg(avgPx));
        });

    parentLay->addWidget(tabWidget_);
}

void AnalysisDialog::setupSignals(QComboBox* rangeCombo, QLabel* startLabel, QLabel* endLabel) {
    auto applyPreset = [this](int index) {
        QDate today = QDate::currentDate();
        switch (index) {
            case kPresetToday:  endDate_->setDate(today); startDate_->setDate(today); break;
            case kPresetWeek:   endDate_->setDate(today); startDate_->setDate(today.addDays(-kPresetWeekDays)); break;
            case kPresetMonth:  endDate_->setDate(today); startDate_->setDate(today.addDays(-kPresetMonthDays)); break;
            case kPresetHalfYear: endDate_->setDate(today); startDate_->setDate(today.addDays(-kPresetHalfYearDays)); break;
        }
    };
    connect(rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this, applyPreset, startLabel, endLabel, rangeCombo](int index) {
            bool isCustom = (index == kPresetCustom);
            startLabel->setVisible(isCustom);
            startDate_->setVisible(isCustom);
            endLabel->setVisible(isCustom);
            endDate_->setVisible(isCustom);
            if (!isCustom) {
                applyPreset(index);
                onAnalyze();
            }
        });
    auto* btnAnalyze = findChild<QPushButton*>();
    connect(btnAnalyze, &QPushButton::clicked, this, &AnalysisDialog::onAnalyze);
}

// ============ onAnalyze ============

void AnalysisDialog::onAnalyze() {
    QDateTime start(startDate_->date(), QTime(0, 0, 0));
    QDateTime end(endDate_->date(), QTime(23, 59, 59));

    auto& db = Database::instance();
    QSqlDatabase sqlDb = db.connection();
    if (!sqlDb.isOpen()) {
        QMessageBox::warning(this, "Error", "Database not open");
        return;
    }

    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    int rangeDays = start.date().daysTo(end.date()) + 1;

    QSqlQuery q(sqlDb);

    q.prepare("SELECT COUNT(*) FROM operations WHERE time >= ? AND time < ?");
    q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
    int countInRange = 0;
    if (q.next()) countInRange = q.value(0).toInt();

    q.exec("SELECT COUNT(*) FROM operations");
    int totalOps = 0;
    if (q.next()) totalOps = q.value(0).toInt();

    q.exec("SELECT COUNT(*) FROM agg_operation_stats");
    int aggCount = 0;
    if (q.next()) aggCount = q.value(0).toInt();

    setWindowTitle(QString("Behavior Analysis - Range: %1, Total: %2, Aggregated: %3")
        .arg(countInRange).arg(totalOps).arg(aggCount));

    // --- Summary cards ---
    bool useAggSummary = (rangeDays > 7);
    
    int activeDays = 0;
    if (useAggSummary) {
        q.prepare("SELECT COUNT(DISTINCT substr(time_bucket,1,10)) FROM agg_time_distribution WHERE time_bucket >= ? AND time_bucket <= ?");
        q.addBindValue(start.toString(kDateFormat)); q.addBindValue(end.toString(kDateFormat));
        q.exec();
        if (q.next()) activeDays = q.value(0).toInt();
    } else {
        q.prepare("SELECT COUNT(DISTINCT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime'))) "
                  "FROM operations WHERE time >= ? AND time < ?");
        q.addBindValue(startMs); q.addBindValue(endMs);
        q.exec();
        if (q.next()) activeDays = q.value(0).toInt();
    }

    int dlgCount = 0;
    if (useAggSummary) {
        q.prepare("SELECT SUM(open_count) FROM agg_dialog_stats WHERE time_bucket >= ? AND time_bucket <= ?");
        q.addBindValue(start.toString(kDateFormat)); q.addBindValue(end.toString(kDateFormat));
        q.exec();
        if (q.next()) dlgCount = q.value(0).toInt();
    } else {
        q.prepare("SELECT COUNT(*) FROM operations WHERE time >= ? AND time < ? AND event_type='dialog_open'");
        q.addBindValue(startMs); q.addBindValue(endMs);
        q.exec();
        if (q.next()) dlgCount = q.value(0).toInt();
    }

    statCards_[0]->setText(QString(
        "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Total Events</span></td></tr>"
        "<tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>%1</span></td></tr></table>").arg(countInRange));
    statCards_[1]->setText(QString(
        "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Active Days</span></td></tr>"
        "<tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>%1</span></td></tr></table>").arg(activeDays));
    statCards_[3]->setText(QString(
        "<table cellspacing=4><tr><td><span style='color:#94A3B8;font-size:11px;'>Dialog Opens</span></td></tr>"
        "<tr><td><span style='font-size:22px;font-weight:700;color:#FFFFFF;'>%1</span></td></tr></table>").arg(dlgCount));

    // --- Update all tabs ---
    operationsTab_->updateData(start, end);
    modulesTab_->updateData(start, end);
    inputTab_->updateData(start, end);
    timeTab_->updateData(start, end);
    heatmapTab_->updateData(start, end);
    dialogTab_->updateData(start, end);
    distanceTab_->updateData(start, end);
}
