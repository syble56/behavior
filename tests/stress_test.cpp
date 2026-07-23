// stress_test.cpp — 压力测试：写入吞吐 / 查询性能 / 聚合 / 并发 / 清理
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QVector>
#include <QHash>
#include <thread>
#include <atomic>
#include <random>
#include <cstdio>
#include <cstring>

#include "storage/database.h"
#include "storage/aggregator.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

static const int TOTAL_OPS = 1000000;
static const int CONCURRENT_THREADS = 4;
static const int RETENTION_DAYS = 180;

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
static const int N_WIN = 8;
static const int N_ACT = 14;
static const int N_CTRL = 10;

static std::mt19937 rng(42);

static Operation randomOp(qint64 ts) {
    std::uniform_int_distribution<int> rWin(0, N_WIN - 1);
    std::uniform_int_distribution<int> rAct(0, N_ACT - 1);
    std::uniform_int_distribution<int> rCtrl(0, N_CTRL - 1);
    std::uniform_int_distribution<int> rEvent(0, 5);
    std::uniform_int_distribution<int> rInput(0, 3);
    std::uniform_int_distribution<int> rSX(1, 1920);
    std::uniform_int_distribution<int> rSY(1, 1080);
    std::uniform_int_distribution<int> rHeat(0, 99);

    Operation op;
    op.sessionId = QString("stress-%1").arg(rAct(rng) % 10);
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
    op.windowTitle = "Test";
    op.isMainWindow = (rWin(rng) == 0);
    op.screenX = rSX(rng);
    op.screenY = rSY(rng);
    op.heatRegion = rHeat(rng);
    if (op.eventType == EventType::DialogClose) {
        op.duration = std::uniform_int_distribution<int>(100, 10000)(rng);
    }
    return op;
}

static void printMs(const char* label, qint64 ms) {
    if (ms < 1000) printf("  %s: %lld ms\n", label, (long long)ms);
    else printf("  %s: %lld s %lld ms\n", label, (long long)(ms/1000), (long long)(ms%1000)); {
        fflush(stdout);
    }
}

static void printSize(const char* label, qint64 bytes) {
    if (bytes < 1024) {
        printf("  %s: %lld B\n", label, (long long)bytes);
    } else if (bytes < 1048576) {
        printf("  %s: %lld KB\n", label, (long long)(bytes/1024));
    } else if (bytes < 1073741824LL) {
        printf("  %s: %lld MB\n", label, (long long)(bytes/1048576));
    } else {
        printf("  %s: %.2f GB\n", label, bytes/1073741824.0);
    }
    fflush(stdout);
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    printf("========== Behavior SDK Stress Test ==========\n");
    printf("Total ops: %d | Threads: %d | Retention: %d days\n\n", TOTAL_OPS, CONCURRENT_THREADS, RETENTION_DAYS);
    fflush(stdout);

    QTemporaryDir dir;
    QString path = dir.path() + "/stress_test.db";
    Config::instance().setDatabasePath(path);
    Config::instance().setBatchSize(1000);
    Config::instance().setRetentionDays(RETENTION_DAYS);

    QDateTime startTime = QDateTime(QDate(2026, 1, 1), QTime(0, 0, 0));
    QDateTime endTime = startTime.addDays(RETENTION_DAYS);
    qint64 startMs = startTime.toMSecsSinceEpoch();
    qint64 rangeMs = endTime.toMSecsSinceEpoch() - startMs;
    std::uniform_int_distribution<qint64> rTime(0, rangeMs);

    // ===== 1. Batch Write =====
    printf("--- 1. Batch Write Throughput ---\n"); fflush(stdout);
    Database::instance().open(path);

    printf("Generating %d operations...\n", TOTAL_OPS); fflush(stdout);
    QList<Operation> ops;
    ops.reserve(TOTAL_OPS);
    for (int i = 0; i < TOTAL_OPS; ++i) {
        ops.append(randomOp(startMs + rTime(rng)));
    }

    printf("Batch inserting...\n"); fflush(stdout);
    QElapsedTimer timer;
    timer.start();
    Database::instance().batchInsert(ops);
    qint64 writeMs = timer.elapsed();
    double opsPerSec = TOTAL_OPS * 1000.0 / writeMs;
    printMs("Time", writeMs);
    printf("  Throughput: %.0f ops/sec\n", opsPerSec); fflush(stdout);

    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT COUNT(*) FROM operations");
    q.next();
    printf("  Stored: %d / %d\n\n", q.value(0).toInt(), TOTAL_OPS); fflush(stdout);

    // ===== 2. Query Performance =====
    printf("--- 2. Query Performance ---\n"); fflush(stdout);

    timer.restart();
    QueryFilter filter;
    filter.startTime = startTime;
    filter.endTime = endTime;
    filter.limit = 1000000;
    auto results = Database::instance().queryOperations(filter);
    printMs("Full range query", timer.elapsed());
    printf("  (%d rows)\n", (int)results.size()); fflush(stdout);

    timer.restart();
    filter.endTime = startTime.addDays(1);
    results = Database::instance().queryOperations(filter);
    printMs("1-day query", timer.elapsed());
    printf("  (%d rows)\n", (int)results.size()); fflush(stdout);

    timer.restart();
    filter.endTime = endTime;
    qint64 cnt = Database::instance().countOperations(filter);
    printMs("Count query", timer.elapsed());
    printf("  (count=%lld)\n", (long long)cnt); fflush(stdout);

    timer.restart();
    filter.eventType = "mouse_click";
    filter.onlyMainWindow = true;
    results = Database::instance().queryOperations(filter);
    printMs("Filtered query (mouse_click+mainwin)", timer.elapsed());
    printf("  (%d rows)\n\n", (int)results.size()); fflush(stdout);

    // ===== 3. Aggregation =====
    printf("--- 3. Aggregation Performance ---\n"); fflush(stdout);
    Aggregator agg;

    timer.restart();
    agg.aggregateRange(startTime, startTime.addDays(30), Aggregator::Granularity::Hour);
    printMs("Aggregate 30 days (hourly)", timer.elapsed());

    timer.restart();
    agg.aggregateRange(startTime, endTime, Aggregator::Granularity::Day);
    printMs("Aggregate 180 days (daily)", timer.elapsed());

    q.exec("SELECT SUM(count) FROM agg_operation_stats");
    q.next();
    printf("  Agg total: %lld\n", (long long)q.value(0).toLongLong()); fflush(stdout);
    q.exec("SELECT COUNT(*) FROM agg_module_stats");
    q.next();
    printf("  Module buckets: %d\n", q.value(0).toInt()); fflush(stdout);
    q.exec("SELECT COUNT(*) FROM agg_heatmap_stats");
    q.next();
    printf("  Heatmap buckets: %d\n\n", q.value(0).toInt()); fflush(stdout);

    // ===== 4. Concurrent Writes =====
    printf("--- 4. Concurrent Writes (%d threads) ---\n", CONCURRENT_THREADS); fflush(stdout);
    // Use a fresh DB for concurrent test
    Database::instance().close();
    QString path2 = dir.path() + "/stress_concurrent.db";
    Config::instance().setDatabasePath(path2);
    Database::instance().open(path2);

    int perThread = 250000;
    std::atomic<int> totalWritten{0};
    timer.restart();

    std::vector<std::thread> threads;
    for (int t = 0; t < CONCURRENT_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            // Each thread gets its own connection via connectionNameForThread()
            QList<Operation> threadOps;
            threadOps.reserve(perThread);
            std::mt19937 localRng(t * 1000 + 42);
            std::uniform_int_distribution<qint64> localRTime(0, rangeMs);
            for (int i = 0; i < perThread; ++i) {
                threadOps.append(randomOp(startMs + localRTime(localRng)));
            }
            Database::instance().batchInsert(threadOps);
            totalWritten += perThread;
        });
    }
    for (auto& th : threads) th.join();
    qint64 concurrentMs = timer.elapsed();

    q = QSqlQuery(Database::instance().connection());
    q.exec("SELECT COUNT(*) FROM operations");
    q.next();
    int concurrentCount = q.value(0).toInt();
    printMs("Time", concurrentMs);
    printf("  Written: %d | Stored: %d\n", totalWritten.load(), concurrentCount);
    printf("  Throughput: %.0f ops/sec\n", totalWritten.load() * 1000.0 / concurrentMs);
    if (concurrentCount == totalWritten.load()) printf("  No data loss\n");
    else printf("  ERROR: lost %d operations!\n", totalWritten.load() - concurrentCount); {
        printf("\n"); fflush(stdout);
    }

    // ===== 5. Data Cleanup =====
    printf("--- 5. Data Cleanup ---\n"); fflush(stdout);
    QDateTime oldTime = startTime.addDays(-20);
    QList<Operation> oldOps;
    for (int i = 0; i < 10000; ++i) {
        oldOps.append(randomOp(oldTime.toMSecsSinceEpoch() + i));
    }
    Database::instance().batchInsert(oldOps);
    q.exec("SELECT COUNT(*) FROM operations");
    q.next();
    int beforeClean = q.value(0).toInt();
    printf("  Before: %d rows\n", beforeClean); fflush(stdout);

    timer.restart();
    int cleaned = Database::instance().cleanOldData(RETENTION_DAYS);
    qint64 cleanMs = timer.elapsed();
    q.exec("SELECT COUNT(*) FROM operations");
    q.next();
    int afterClean = q.value(0).toInt();
    printMs("Cleanup time", cleanMs);
    printf("  Cleaned: %d | After: %d\n\n", cleaned, afterClean); fflush(stdout);

    // ===== 6. DB Size =====
    printf("--- 6. Database Size ---\n"); fflush(stdout);
    Database::instance().close();
    QFile dbFile(path2);
    qint64 dbSize = dbFile.size();
    printSize("DB size", dbSize);
    printf("  Per op: %.1f bytes\n", (double)dbSize / afterClean); fflush(stdout);
    QFile walFile(path2 + "-wal");
    if (walFile.exists()) printSize("WAL size", walFile.size());
    printf("\n"); fflush(stdout);

    // ===== Summary =====
    printf("========== Summary ==========\n");
    printf("Batch write:      %.0f ops/sec\n", opsPerSec);
    printf("Concurrent write:  %.0f ops/sec\n", totalWritten.load() * 1000.0 / concurrentMs);
    printf("Cleanup:           %lld ms (%d rows)\n", (long long)cleanMs, cleaned);
    printSize("DB size", dbSize);
    printf("=============================\n");
    fflush(stdout);

    return 0;
}
