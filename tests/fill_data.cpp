// fill_data.cpp — 向生产数据库填充100万条测试数据
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDateTime>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cstdio>
#include <random>

#include "storage/database.h"
#include "core/config.h"
#include "core/types.h"

using namespace ui_shared::behavior;

static const char* WINDOW_CLASSES[] = {
    "QMainWindow", "SettingsDialog", "OptionsDialog", "HelpWindow",
    "ExportDialog", "LoginWindow", "DashboardWindow", "ReportDialog"
};
static const char* ACTION_NAMES[] = {
    "save", "open", "close", "delete", "copy", "paste", "cut",
    "undo", "redo", "search", "refresh", "export", "import", "print"
};
static const char* CONTROL_NAMES[] = {
    "btnSave", "btnOpen", "btnClose", "btnDelete", "btnExport",
    "txtInput", "comboSelect", "listItems", "tableData", "treeView"
};

static Operation randomOp(qint64 ts, std::mt19937& rng) {
    std::uniform_int_distribution<int> rWin(0, 7);
    std::uniform_int_distribution<int> rAct(0, 13);
    std::uniform_int_distribution<int> rCtrl(0, 9);
    std::uniform_int_distribution<int> rEvent(0, 5);
    std::uniform_int_distribution<int> rInput(0, 3);
    std::uniform_int_distribution<int> rSX(1, 1920);
    std::uniform_int_distribution<int> rSY(1, 1080);
    std::uniform_int_distribution<int> rHeat(0, 99);

    Operation op;
    op.sessionId = QString("sess-%1").arg(rAct(rng) % 10);
    op.timestamp = ts;
    int et = rEvent(rng);
    switch (et) {
        case 0: op.eventType = EventType::MouseClick; break;
        case 1: op.eventType = EventType::TouchTap; break;
        case 2: op.eventType = EventType::Shortcut; break;
        case 3: op.eventType = EventType::DialogOpen; break;
        case 4: op.eventType = EventType::DialogClose; break;
        default: op.eventType = EventType::AreaClick; break;
    }
    int im = rInput(rng);
    switch (im) {
        case 0: op.inputMethod = InputMethod::Mouse; break;
        case 1: op.inputMethod = InputMethod::Keyboard; break;
        case 2: op.inputMethod = InputMethod::Touch; break;
        default: op.inputMethod = InputMethod::Knob; break;
    }
    op.controlClass = "QPushButton";
    op.controlName = CONTROL_NAMES[rCtrl(rng)];
    op.actionName = ACTION_NAMES[rAct(rng)];
    op.windowClass = WINDOW_CLASSES[rWin(rng)];
    op.windowTitle = "Window";
    op.isMainWindow = (rWin(rng) == 0);
    op.screenX = rSX(rng);
    op.screenY = rSY(rng);
    op.heatRegion = rHeat(rng);
    if (op.eventType == EventType::DialogClose) {
        op.duration = std::uniform_int_distribution<int>(100, 10000)(rng);
    }
    return op;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // 使用生产数据库路径
    QString dbPath;
    if (argc > 1) {
        dbPath = argv[1];
    } else {
        dbPath = "C:/Users/admin/AppData/Local/Syble/BehaviorDemo/behavior.db";
    }
    QFileInfo fi(dbPath);
    printf("DB path: %s\n", qPrintable(dbPath));
    printf("DB exists: %s\n", fi.exists() ? "yes" : "no");
    if (fi.exists()) printf("DB size before: %lld KB\n", (long long)(fi.size() / 1024));
    fflush(stdout);

    Config::instance().setDatabasePath(dbPath);
    Config::instance().setBatchSize(1000);
    Config::instance().setRetentionDays(180);

    Database::instance().open(dbPath);

    // 当前数据量
    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT COUNT(*) FROM operations");
    q.next();
    int existing = q.value(0).toInt();
    printf("Existing rows: %d\n\n", existing);
    fflush(stdout);

    // 生成100万条数据，时间范围：过去180天
    const int TOTAL = 1000000;
    QDateTime endTime = QDateTime::currentDateTime();
    QDateTime startTime = endTime.addDays(-180);
    qint64 startMs = startTime.toMSecsSinceEpoch();
    qint64 rangeMs = endTime.toMSecsSinceEpoch() - startMs;

    printf("Filling %d operations (180-day range)...\n", TOTAL);
    fflush(stdout);

    QElapsedTimer timer;
    timer.start();

    // 分批插入，每批10000条
    const int BATCH = 10000;
    std::mt19937 rng(42);
    std::uniform_int_distribution<qint64> rTime(0, rangeMs);

    for (int i = 0; i < TOTAL; i += BATCH) {
        QList<Operation> ops;
        ops.reserve(BATCH);
        for (int j = 0; j < BATCH && i + j < TOTAL; ++j) {
            ops.append(randomOp(startMs + rTime(rng), rng));
        }
        if (!Database::instance().batchInsert(ops)) {
            printf("ERROR: batch insert failed at offset %d\n", i);
            fflush(stdout);
            return 1;
        }
        if ((i / BATCH) % 10 == 0) {
            printf("  %d / %d (%.0f%%)\n", i + BATCH, TOTAL, (i + BATCH) * 100.0 / TOTAL);
            fflush(stdout);
        }
    }

    qint64 elapsed = timer.elapsed();
    printf("Done in %lld ms (%.0f ops/sec)\n", (long long)elapsed, TOTAL * 1000.0 / elapsed);

    // 验证
    q.exec("SELECT COUNT(*) FROM operations");
    q.next();
    int total = q.value(0).toInt();
    printf("Total rows now: %d\n", total);

    QFile f(dbPath);
    printf("DB size after: %lld MB\n", (long long)(f.size() / 1048576));
    fflush(stdout);

    Database::instance().close();
    return 0;
}
