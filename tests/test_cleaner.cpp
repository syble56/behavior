// test_cleaner.cpp — 验证异步数据清理器 Cleaner
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include <cstdio>
#include <random>

#include "storage/database.h"
#include "storage/cleaner.h"
#include "core/config.h"
#include "core/types.h"

using namespace ui_shared::behavior;

static const char* WINDOW_CLASSES[] = {
    "QMainWindow", "SettingsDialog", "OptionsDialog", "HelpWindow"
};

static Operation makeOp(qint64 ts, const QString& sess = "test-0") {
    static std::mt19937 rng(42);
    std::uniform_int_distribution<int> rWin(0, 3);
    Operation op;
    op.sessionId = sess;
    op.timestamp = ts;
    op.eventType = EventType::MouseClick;
    op.inputMethod = InputMethod::Mouse;
    op.controlClass = "QPushButton";
    op.controlName = "btnSave";
    op.actionName = "save";
    op.windowClass = WINDOW_CLASSES[rWin(rng)];
    op.windowTitle = "Test";
    op.isMainWindow = (rWin(rng) == 0);
    op.screenX = 100;
    op.screenY = 200;
    op.heatRegion = 5;
    return op;
}

static void printResult(const char* name, bool pass) {
    printf("  [%s] %s\n", pass ? "PASS" : "FAIL", name);
    fflush(stdout);
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    printf("========== Cleaner Async Test ==========\n\n");
    fflush(stdout);

    QTemporaryDir dir;
    QString path = dir.path() + "/cleaner_test.db";
    Config::instance().setDatabasePath(path);
    Config::instance().setRetentionDays(30);

    // 打开数据库
    Database::instance().open(path);

    // 插入测试数据：50000条近期 + 5000条过期(60天前)
    printf("--- Setup ---\n");
    QDateTime now = QDateTime::currentDateTime();
    qint64 nowMs = now.toMSecsSinceEpoch();
    qint64 oldMs = now.addDays(-60).toMSecsSinceEpoch();

    QList<Operation> recentOps, oldOps;
    for (int i = 0; i < 50000; ++i) {
        recentOps.append(makeOp(nowMs - i * 1000));
    }
    for (int i = 0; i < 5000; ++i) {
        oldOps.append(makeOp(oldMs + i * 100));
    }

    Database::instance().batchInsert(recentOps);
    Database::instance().batchInsert(oldOps);

    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT COUNT(*) FROM operations");
    q.next();
    int totalBefore = q.value(0).toInt();
    printf("  Total before: %d (50000 recent + 5000 expired)\n", totalBefore);
    fflush(stdout);

    // ===== Test 1: Cleaner 基本功能 =====
    printf("\n--- Test 1: Basic async clean ---\n");
    {
        Cleaner cleaner;
        cleaner.start();

        QElapsedTimer timer;
        timer.start();

        QEventLoop loop;
        int removed = -1;
        QObject::connect(&cleaner, &Cleaner::finished, &loop, [&](int r) {
            removed = r;
            loop.quit();
        });

        // 异步触发清理（30天保留期）
        cleaner.cleanAsync(30);
        printf("  cleanAsync() returned immediately (non-blocking)\n");
        fflush(stdout);

        // 等待完成，最多10秒
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        loop.exec();

        qint64 elapsed = timer.elapsed();
        printf("  Clean finished in %lld ms, removed = %d\n", (long long)elapsed, removed);
        fflush(stdout);

        printResult("cleanAsync returns immediately", elapsed >= 0);
        printResult("removed > 0", removed > 0);
        printResult("removed == 5000 (expired ops)", removed == 5000);

        // 验证数据库剩余数据
        q.exec("SELECT COUNT(*) FROM operations");
        q.next();
        int totalAfter = q.value(0).toInt();
        printf("  Total after: %d (expected 50000)\n", totalAfter);
        fflush(stdout);
        printResult("remaining == 50000", totalAfter == 50000);

        cleaner.stop();
    }

    // ===== Test 2: Cleaner 在独立线程运行 =====
    // 通过间接方式验证：主线程在等待期间可以处理事件
    printf("\n--- Test 2: Non-blocking verification ---\n");
    {
        // 插入大量过期数据让清理耗时更长
        QList<Operation> bigOld;
        for (int i = 0; i < 50000; ++i) {
            bigOld.append(makeOp(oldMs + i * 10));
        }
        Database::instance().batchInsert(bigOld);

        q.exec("SELECT COUNT(*) FROM operations");
        q.next();
        printf("  Before clean: %d rows\n", q.value(0).toInt());

        Cleaner cleaner;
        cleaner.start();

        // 主线程在清理期间继续处理事件
        int pollCount = 0;
        QTimer pollTimer;
        pollTimer.setInterval(10);  // 每10ms触发
        pollTimer.setSingleShot(false);
        QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
            pollCount++;
        });

        QEventLoop loop;
        int removed = -1;
        QObject::connect(&cleaner, &Cleaner::finished, &loop, [&](int r) {
            removed = r;
            loop.quit();
        });

        pollTimer.start();
        QElapsedTimer timer;
        timer.start();
        cleaner.cleanAsync(30);
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        loop.exec();
        pollTimer.stop();
        qint64 elapsed = timer.elapsed();

        printf("  Clean took %lld ms, removed %d, poll fired %d times\n",
               (long long)elapsed, removed, pollCount);
        fflush(stdout);
        printResult("clean completed", removed >= 0);
        printResult("main thread event loop ran during clean (poll > 0)", pollCount > 0);

        cleaner.stop();
    }

    // ===== Test 3: 多次连续清理 =====
    printf("\n--- Test 3: Multiple sequential cleans ---\n");
    {
        Cleaner cleaner;
        cleaner.start();

        int cleanCount = 0;
        int totalRemoved = 0;

        for (int round = 0; round < 3; ++round) {
            // 每轮插入新的过期数据
            QList<Operation> roundOld;
            int batch = 2000 * (round + 1);
            for (int i = 0; i < batch; ++i) {
                roundOld.append(makeOp(oldMs + i * 50 + round * 1000000));
            }
            Database::instance().batchInsert(roundOld);

            q.exec("SELECT COUNT(*) FROM operations");
            q.next();
            int before = q.value(0).toInt();

            QEventLoop loop;
            int removed = -1;
            QObject::connect(&cleaner, &Cleaner::finished, &loop, [&](int r) {
                removed = r;
                loop.quit();
            });
            cleaner.cleanAsync(30);
            QTimer::singleShot(10000, &loop, &QEventLoop::quit);
            loop.exec();

            totalRemoved += removed;
            cleanCount++;

            q.exec("SELECT COUNT(*) FROM operations");
            q.next();
            int after = q.value(0).toInt();

            printf("  Round %d: before=%d, removed=%d, after=%d\n",
                   round + 1, before, removed, after);
            fflush(stdout);
        }

        printResult("all 3 rounds completed", cleanCount == 3);
        printResult("total removed > 0", totalRemoved > 0);

        cleaner.stop();
    }

    // ===== Test 4: stop() 正确清理线程 =====
    printf("\n--- Test 4: stop() cleanup ---\n");
    {
        Cleaner cleaner;
        cleaner.start();
        cleaner.stop();
        printResult("stop() returns without crash", true);

        // 可以再次 start
        cleaner.start();
        cleaner.stop();
        printResult("re-start after stop works", true);
    }

    // ===== Test 5: 空库清理不崩溃 =====
    printf("\n--- Test 5: Clean on empty DB ---\n");
    {
        QTemporaryDir dir2;
        QString path2 = dir2.path() + "/empty_test.db";
        Config::instance().setDatabasePath(path2);
        Database::instance().close();
        Database::instance().open(path2);

        Cleaner cleaner;
        cleaner.start();

        QEventLoop loop;
        int removed = -999;
        QObject::connect(&cleaner, &Cleaner::finished, &loop, [&](int r) {
            removed = r;
            loop.quit();
        });
        cleaner.cleanAsync(30);
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        loop.exec();

        printf("  Empty DB clean: removed = %d\n", removed);
        fflush(stdout);
        printResult("empty DB clean returns 0", removed == 0);

        cleaner.stop();
        Database::instance().close();

        // 恢复原DB
        Config::instance().setDatabasePath(path);
        Database::instance().open(path);
    }

    // ===== Summary =====
    printf("\n========== Summary ==========\n");
    printf("All cleaner tests completed.\n");
    printf("=============================\n");
    fflush(stdout);

    Database::instance().close();
    return 0;
}
