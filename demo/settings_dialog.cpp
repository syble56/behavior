#include "settings_dialog.h"
#include "core/behavior_analytics.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QStyle>

// ============ SettingsDialog ============

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("System Settings");
    setMinimumWidth(400);
    auto* lay = new QVBoxLayout(this);

    auto* dbGroup = new QGroupBox("Database", this);
    auto* dbLay = new QVBoxLayout(dbGroup);

    auto* pathLay = new QHBoxLayout();
    pathLay->addWidget(new QLabel("Path:", this));
    auto* pathEdit = new QLineEdit(this);
    pathEdit->setPlaceholderText("Default path");
    pathLay->addWidget(pathEdit);
    auto* browseBtn = new QPushButton("Browse...", this);
    pathLay->addWidget(browseBtn);
    dbLay->addLayout(pathLay);

    auto* retentionLay = new QHBoxLayout();
    retentionLay->addWidget(new QLabel("Retention (days):", this));
    auto* retentionSpin = new QSpinBox(this);
    retentionSpin->setRange(1, 365);
    retentionSpin->setValue(180);
    retentionLay->addWidget(retentionSpin);
    retentionLay->addStretch();
    dbLay->addLayout(retentionLay);

    lay->addWidget(dbGroup);

    auto* collectGroup = new QGroupBox("Collection", this);
    auto* collectLay = new QVBoxLayout(collectGroup);

    auto* batchLay = new QHBoxLayout();
    batchLay->addWidget(new QLabel("Batch size:", this));
    auto* batchSpin = new QSpinBox(this);
    batchSpin->setRange(10, 1000);
    batchSpin->setValue(100);
    batchLay->addWidget(batchSpin);
    batchLay->addStretch();
    collectLay->addLayout(batchLay);

    auto* enableCheck = new QCheckBox("Enable collection", this);
    enableCheck->setChecked(true);
    collectLay->addWidget(enableCheck);

    lay->addWidget(collectGroup);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    lay->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ============ SystemButtonsDialog ============

SystemButtonsDialog::SystemButtonsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("System Actions");
    setMinimumWidth(300);
    auto* lay = new QVBoxLayout(this);

    lay->addWidget(new QLabel("Quick actions:", this));

    auto* btnExport = new QPushButton("Export Data", this);
    btnExport->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    lay->addWidget(btnExport);

    auto* btnImport = new QPushButton("Import Data", this);
    btnImport->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    lay->addWidget(btnImport);

    auto* btnClear = new QPushButton("Clear Data", this);
    btnClear->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    lay->addWidget(btnClear);

    auto* btnBackup = new QPushButton("Backup Database", this);
    lay->addWidget(btnBackup);

    lay->addStretch();

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    lay->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(btnExport, &QPushButton::clicked, this, []{
        QMessageBox::information(nullptr, "Export", "Data export feature");
    });
    connect(btnClear, &QPushButton::clicked, this, []{
        auto ret = QMessageBox::question(nullptr, "Confirm Clear",
            "This will delete all operation records and aggregation data. This cannot be undone.\n\n"
            "Are you sure?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;

        auto& db = ui_shared::behavior::Database::instance();
        auto sqlDb = db.connection();

        QStringList tables = {
            "operations", "sessions",
            "agg_operation_stats", "agg_module_stats", "agg_input_stats",
            "agg_heatmap_stats", "agg_dialog_stats", "agg_time_distribution"
        };
        for (const auto& t : tables) {
            QSqlQuery q(sqlDb);
            q.exec(QStringLiteral("DELETE FROM %1").arg(t));
        }

        QMessageBox::information(nullptr, "Done", "All data cleared.");
    });
}
