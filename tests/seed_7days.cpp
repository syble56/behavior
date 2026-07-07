// seed_7days.cpp — 清空数据库 + 灌入最近7天模拟数据 + 聚合
#include <QCoreApplication>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QList>
#include <QDebug>
#include <QRandomGenerator>
#include <cmath>
#include <algorithm>

#include "core/types.h"
#include "storage/database.h"
#include "storage/aggregator.h"
#include "core/config.h"

using namespace ui_shared::behavior;

static QTextStream g_log;

static void info(const QString& s) {
    g_log << s << "\n";
    g_log.flush();
}

static const QStringList CONTROL_CLASSES = {
    "QPushButton", "QToolButton", "QLineEdit", "QComboBox",
    "QCheckBox", "QRadioButton", "QTabBar", "QTableView",
    "QTreeView", "QListWidget", "QSpinBox", "QSlider"
};

static const QStringList CONTROL_NAMES = {
    "btnOk", "btnCancel", "btnSave", "btnOpen", "btnClose", "btnNew",
    "btnDelete", "btnEdit", "btnSearch", "btnRefresh", "btnExport",
    "btnPrint", "btnSettings", "btnHelp", "btnApply", "btnReset",
    "editName", "editSearch", "editPath", "editValue",
    "comboType", "comboLang", "checkEnable", "radioOption1"
};

static const QStringList WINDOW_CLASSES = {
    "MainWindow", "SettingsDialog", "ExportDialog", "SearchPanel",
    "LoginDialog", "AboutDialog", "PropertyEditor", "ChartView"
};

static const QStringList ACTION_NAMES = {
    "file.new", "file.open", "file.save", "file.saveAs", "file.close",
    "edit.copy", "edit.cut", "edit.paste", "edit.undo", "edit.redo",
    "edit.find", "edit.selectAll", "view.zoomIn", "view.zoomOut",
    "view.fullscreen", "tools.settings", "tools.export", "tools.print",
    "help.about", "help.docs"
};

static QRandomGenerator rng(QDateTime::currentDateTime().toMSecsSinceEpoch());

static QString pick(const QStringList& list) {
    return list.at(rng.bounded(list.size()));
}

static Operation genOperation(qint64 ts) {
    Operation op;
    op.sessionId = QStringLiteral("seed-%1").arg(ts / 86400000);
    op.timestamp = ts;

    int r = rng.bounded(100);
    if (r < 55) {
        op.eventType = EventType::MouseClick;
        op.inputMethod = InputMethod::Mouse;
    } else if (r < 75) {
        op.eventType = EventType::Shortcut;
        op.inputMethod = InputMethod::Keyboard;
    } else if (r < 90) {
        op.eventType = EventType::AreaClick;
        op.inputMethod = InputMethod::Mouse;
    } else if (r < 97) {
        op.eventType = EventType::DialogOpen;
        op.inputMethod = InputMethod::Mouse;
    } else {
        op.eventType = EventType::DialogClose;
        op.inputMethod = InputMethod::Mouse;
        op.duration = rng.bounded(500, 10000);
    }

    op.controlClass = pick(CONTROL_CLASSES);
    op.controlName = pick(CONTROL_NAMES);
    op.controlText = "";
    op.controlPath = "";

    if (op.eventType == EventType::Shortcut) {
        op.actionName = pick(ACTION_NAMES);
        op.keySequence = "Ctrl+S";
    }

    op.windowClass = pick(WINDOW_CLASSES);
    op.windowTitle = op.windowClass;
    op.isMainWindow = (op.windowClass == "MainWindow");

    op.screenX = rng.bounded(0, 1920);
    op.screenY = rng.bounded(0, 1080);
    op.heatRegion = (op.screenY / 108) * 10 + (op.screenX / 192);
    if (op.heatRegion < 0 || op.heatRegion > 99) op.heatRegion = 0;

    return op;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Syble");
    QCoreApplication::setApplicationName("BehaviorDemo");

    QFile logFile(QDir::currentPath() + "/seed_log.txt");
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);
    g_log.setDevice(&logFile);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(path);
    path += "/behavior.db";

    info(QStringLiteral("Database: %1").arg(path));

    // 删除旧数据库
    bool removed = QFile::remove(path);
    QFile::remove(path + "-wal");
    QFile::remove(path + "-shm");
    QFile::remove(path + "-journal");
    if (!removed) {
        info("WARNING: Failed to remove old database! File may be locked.");
        // Try to open and drop all tables instead
        Database::instance().open(path);
        QSqlDatabase db0 = Database::instance().connection();
        QSqlQuery qd(db0);
        for (const auto& t : QStringList{"operations","sessions","agg_operation_stats","agg_module_stats",
              "agg_input_stats","agg_heatmap_stats","agg_dialog_stats","agg_time_distribution"}) {
            qd.exec(QStringLiteral("DELETE FROM %1").arg(t));
        }
        Database::instance().close();
        info("Cleared all tables instead.");
    } else {
        info("Old database removed.");
    }

    Config::instance().setDatabasePath(path);
    if (!Database::instance().open(path)) {
        info("FAILED to open database!");
        return 1;
    }

    QDateTime now = QDateTime::currentDateTime();
    QDateTime start7d = now.addDays(-7);
    qint64 startMs = start7d.toMSecsSinceEpoch();
    qint64 endMs = now.toMSecsSinceEpoch();

    info(QStringLiteral("Generating 7 days: %1 ~ %2")
         .arg(start7d.toString("yyyy-MM-dd HH:mm"))
         .arg(now.toString("yyyy-MM-dd HH:mm")));

    QList<Operation> batch;
    int totalOps = 0;

    for (qint64 ts = startMs; ts < endMs; ts += 60000) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts);
        int hour = dt.time().hour();
        int dow = dt.date().dayOfWeek();

        int opsThisMinute = 0;
        if (dow <= 5) {
            if (hour >= 9 && hour < 12)       opsThisMinute = rng.bounded(2, 6);
            else if (hour >= 14 && hour < 18) opsThisMinute = rng.bounded(1, 5);
            else if (hour >= 12 && hour < 14) opsThisMinute = rng.bounded(0, 2);
            else if (hour >= 7 && hour < 9)   opsThisMinute = rng.bounded(0, 2);
            else if (hour >= 18 && hour < 21) opsThisMinute = rng.bounded(0, 1);
        } else {
            if (hour >= 10 && hour < 17)       opsThisMinute = rng.bounded(0, 3);
        }

        for (int i = 0; i < opsThisMinute; ++i) {
            qint64 opTs = std::min(ts + rng.bounded(0, 60000), endMs - 1);
            batch.append(genOperation(opTs));
            totalOps++;
        }

        if (batch.size() >= 5000) {
            Database::instance().batchInsert(batch);
            batch.clear();
        }
    }

    if (!batch.isEmpty()) {
        Database::instance().batchInsert(batch);
    }

    info(QStringLiteral("Total operations: %1").arg(totalOps));

    // 会话
    for (int d = 0; d < 7; ++d) {
        QDateTime dayStart = now.addDays(-(d + 1));
        Session s;
        s.id = QStringLiteral("seed-%1").arg(dayStart.toMSecsSinceEpoch() / 86400000);
        s.startTime = dayStart.toMSecsSinceEpoch();
        s.endTime = dayStart.addSecs(8 * 3600).toMSecsSinceEpoch();
        s.durationSeconds = 8 * 3600;
        s.operationCount = totalOps / 7;
        Database::instance().insertSession(s);
    }
    info("Sessions: 7");

    // 聚合
    info("Aggregating hour...");
    Aggregator agg;
    agg.aggregateRange(start7d, now, Aggregator::Granularity::Hour);
    info("Aggregating day...");
    agg.aggregateRange(start7d, now, Aggregator::Granularity::Day);
    info("Aggregation done.");

    // 统计
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);

    auto countRow = [&](const QString& sql) -> qint64 {
        q.exec(sql);
        return (q.next()) ? q.value(0).toLongLong() : -1;
    };

    info(QStringLiteral("  operations: %1").arg(countRow("SELECT COUNT(*) FROM operations")));
    info(QStringLiteral("  agg_operation_stats: %1").arg(countRow("SELECT COUNT(*) FROM agg_operation_stats")));
    info(QStringLiteral("  agg_module_stats: %1").arg(countRow("SELECT COUNT(*) FROM agg_module_stats")));
    info(QStringLiteral("  agg_input_stats: %1").arg(countRow("SELECT COUNT(*) FROM agg_input_stats")));
    info(QStringLiteral("  agg_heatmap_stats: %1").arg(countRow("SELECT COUNT(*) FROM agg_heatmap_stats")));
    info(QStringLiteral("  agg_dialog_stats: %1").arg(countRow("SELECT COUNT(*) FROM agg_dialog_stats")));
    info(QStringLiteral("  agg_time_distribution: %1").arg(countRow("SELECT COUNT(*) FROM agg_time_distribution")));

    // 每天操作数
    info("Per-day counts:");
    q.exec("SELECT time_bucket, SUM(count) FROM agg_input_stats WHERE granularity='day' GROUP BY time_bucket ORDER BY time_bucket");
    while (q.next()) {
        info(QStringLiteral("  %1 : %2").arg(q.value(0).toString()).arg(q.value(1).toInt()));
    }

    // Top 5 action
    info("Top 5 actions:");
    q.exec("SELECT action_key, SUM(count) as total FROM agg_operation_stats WHERE granularity='day' GROUP BY action_key ORDER BY total DESC LIMIT 5");
    while (q.next()) {
        info(QStringLiteral("  %1 : %2").arg(q.value(0).toString()).arg(q.value(1).toInt()));
    }

    // Top 5 module
    info("Top 5 modules:");
    q.exec("SELECT module_class, SUM(count) as total FROM agg_module_stats WHERE granularity='day' GROUP BY module_class ORDER BY total DESC LIMIT 5");
    while (q.next()) {
        info(QStringLiteral("  %1 : %2").arg(q.value(0).toString()).arg(q.value(1).toInt()));
    }

    Database::instance().close();
    info("Done! Open the demo app to verify.");
    return 0;
}
