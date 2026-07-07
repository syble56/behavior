// TestAggregator.cpp — 聚合逻辑正确性验证
#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QDateTime>
#include <QSignalSpy>

#include "storage/database.h"
#include "storage/aggregator.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

namespace {
Operation makeOp(qint64 ts, EventType et = EventType::MouseClick,
                 InputMethod im = InputMethod::Mouse,
                 const QString& win = "MainWindow", bool mainWin = true,
                 const QString& action = "", int heatRegion = 42) {
    Operation op;
    op.sessionId = "agg-test";
    op.timestamp = ts;
    op.eventType = et;
    op.inputMethod = im;
    op.controlClass = "QPushButton";
    op.controlName = "btnOk";
    op.actionName = action;
    op.windowClass = win;
    op.isMainWindow = mainWin;
    op.screenX = 100;
    op.screenY = 200;
    op.heatRegion = heatRegion;
    return op;
}

qint64 aggCount(const QString& table) {
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table));
    return (q.next()) ? q.value(0).toLongLong() : -1;
}

qint64 aggSum(const QString& table, const QString& col) {
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT SUM(%1) FROM %2").arg(col, table));
    return (q.next()) ? q.value(0).toLongLong() : -1;
}
} // namespace

class TestAggregator : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // ===== 操作聚合 =====
    void testAggregateOperationsByHour();
    void testAggregateOperationsByDay();
    void testAggregateOperationsEmptyRange();
    void testAggregateOperationsActionKeyFallback();

    // ===== 模块聚合 =====
    void testAggregateModules();
    void testAggregateModulesUnknownClass();

    // ===== 输入方式聚合 =====
    void testAggregateInputs();
    void testAggregateInputsAllMethods();

    // ===== 热力图聚合 =====
    void testAggregateHeatmapOnlyMainWindow();
    void testAggregateHeatmapExcludesNonClickEvents();

    // ===== 对话框聚合 =====
    void testAggregateDialogs();
    void testAggregateDialogsDurationCalc();

    // ===== 时间分布 =====
    void testTimeDistribution();
    void testTimeDistributionHourlyBuckets();

    // ===== 幂等性 =====
    void testAggregateIdempotent();

    // ===== 信号 =====
    void testAggregationCompletedSignal();

    // ===== 粒度切换 =====
    void testGranularityFormat();

    // ===== 跨天边界 =====
    void testCrossDayHourBuckets();
    void testCrossDayDayBuckets();
    void testCrossDayTimeDistribution();
    void testCrossDayMidnightExact();
    void testCrossDayOperationsSameAction();
    void testCrossDayModuleConsistency();
    void testCrossDayHeatmapSplit();
    void testCrossDayDialogAcrossMidnight();
    void testCrossDayIdempotent();

private:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
    QDateTime baseTime_;
};

void TestAggregator::init() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/agg_test.db";
    Config::instance().setDatabasePath(path_);
    Database::instance().open(path_);
    // 用一个固定时间点：2026-06-15 10:30:00
    baseTime_ = QDateTime(QDate(2026, 6, 15), QTime(10, 30, 0));
}

void TestAggregator::cleanup() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

// ========== 操作聚合 ==========

void TestAggregator::testAggregateOperationsByHour() {
    // 在同一小时内插入 5 条操作
    qint64 base = baseTime_.toMSecsSinceEpoch();
    QList<Operation> ops;
    for (int i = 0; i < 5; ++i)
        ops << makeOp(base + i * 60000, EventType::MouseClick, InputMethod::Mouse,
                      "MainWindow", true, "save", 10);
    Database::instance().batchInsert(ops);

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT count FROM agg_operation_stats WHERE action_key='save' AND granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 5);
}

void TestAggregator::testAggregateOperationsByDay() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    QList<Operation> ops;
    for (int i = 0; i < 3; ++i)
        ops << makeOp(base + i * 3600000, EventType::MouseClick, InputMethod::Mouse,
                      "MainWindow", true, "open", 10);
    Database::instance().batchInsert(ops);

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addDays(1), Aggregator::Granularity::Day);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT count FROM agg_operation_stats WHERE action_key='open' AND granularity='day'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 3);
}

void TestAggregator::testAggregateOperationsEmptyRange() {
    // 没数据，聚合不应崩溃，结果应为0条
    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);
    QCOMPARE(aggCount("agg_operation_stats"), qint64(0));
}

void TestAggregator::testAggregateOperationsActionKeyFallback() {
    // action_name 为空 → 回退到 control_name → control_class → event_type
    qint64 ts = baseTime_.toMSecsSinceEpoch();
    Operation op;
    op.sessionId = "fallback";
    op.timestamp = ts;
    op.eventType = EventType::MouseClick;
    op.inputMethod = InputMethod::Mouse;
    op.controlClass = "QToolButton";
    op.controlName = "btnRefresh";  // action_name 空，应回退到 control_name
    op.windowClass = "MainWindow";
    op.isMainWindow = true;
    Database::instance().insertOperation(op);

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT action_key FROM agg_operation_stats WHERE granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("btnRefresh"));
}

// ========== 模块聚合 ==========

void TestAggregator::testAggregateModules() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().batchInsert({
        makeOp(base, EventType::MouseClick, InputMethod::Mouse, "SettingsDialog", false),
        makeOp(base + 1, EventType::MouseClick, InputMethod::Mouse, "SettingsDialog", false),
        makeOp(base + 2, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true),
    });

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT module_class, count FROM agg_module_stats WHERE granularity='hour' ORDER BY count DESC");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("SettingsDialog"));
    QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("MainWindow"));
    QCOMPARE(q.value(1).toInt(), 1);
}

void TestAggregator::testAggregateModulesUnknownClass() {
    // window_class 为空 → 应聚合为 'unknown'
    qint64 ts = baseTime_.toMSecsSinceEpoch();
    Operation op = makeOp(ts);
    op.windowClass = "";  // 空
    Database::instance().insertOperation(op);

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT module_class FROM agg_module_stats WHERE granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("unknown"));
}

// ========== 输入方式聚合 ==========

void TestAggregator::testAggregateInputs() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().batchInsert({
        makeOp(base, EventType::MouseClick, InputMethod::Mouse),
        makeOp(base + 1, EventType::MouseClick, InputMethod::Mouse),
        makeOp(base + 2, EventType::Shortcut, InputMethod::Keyboard),
    });

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT input_method, count FROM agg_input_stats WHERE granularity='hour' ORDER BY count DESC");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("mouse"));
    QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("keyboard"));
    QCOMPARE(q.value(1).toInt(), 1);
}

void TestAggregator::testAggregateInputsAllMethods() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().batchInsert({
        makeOp(base, EventType::MouseClick, InputMethod::Mouse),
        makeOp(base + 1, EventType::TouchTap, InputMethod::Touch),
        makeOp(base + 2, EventType::Shortcut, InputMethod::Keyboard),
    });

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QCOMPARE(aggCount("agg_input_stats"), qint64(3));
}

// ========== 热力图聚合 ==========

void TestAggregator::testAggregateHeatmapOnlyMainWindow() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().batchInsert({
        makeOp(base, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "", 10),
        makeOp(base + 1, EventType::MouseClick, InputMethod::Mouse, "Dialog", false, "", 20),
    });

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    // 只有 is_main_window=1 的才进热力图
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT heat_region, count FROM agg_heatmap_stats WHERE granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 10);
    QCOMPARE(q.value(1).toInt(), 1);
    QVERIFY(!q.next()); // 只有一条
}

void TestAggregator::testAggregateHeatmapExcludesNonClickEvents() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().batchInsert({
        makeOp(base, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "", 10),
        makeOp(base + 1, EventType::Shortcut, InputMethod::Keyboard, "MainWindow", true, "", 10),
        makeOp(base + 2, EventType::DialogOpen, InputMethod::Mouse, "MainWindow", true, "", 10),
    });

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    // 只有 mouse_click 进热力图
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT SUM(count) FROM agg_heatmap_stats WHERE granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
}

// ========== 对话框聚合 ==========

void TestAggregator::testAggregateDialogs() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // dialog_open + dialog_close 事件
    Operation op0 = makeOp(base, EventType::DialogOpen, InputMethod::Mouse, "SettingsDialog", false);
    op0.windowTitle = "SettingsDialog";
    Operation op1 = makeOp(base + 1, EventType::DialogClose, InputMethod::Mouse, "SettingsDialog", false);
    op1.windowTitle = "SettingsDialog";
    op1.duration = 5000;
    Operation op2 = makeOp(base + 2, EventType::DialogClose, InputMethod::Mouse, "SettingsDialog", false);
    op2.windowTitle = "SettingsDialog";
    op2.duration = 3000;
    Database::instance().batchInsert({op0, op1, op2});

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT open_count, total_duration, avg_duration FROM agg_dialog_stats WHERE dialog_class='SettingsDialog'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);       // 1次打开
    QCOMPARE(q.value(1).toInt(), 8000);     // 5000+3000
    QCOMPARE(q.value(2).toInt(), 4000);     // avg = 4000
}

void TestAggregator::testAggregateDialogsDurationCalc() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // duration 为 nullopt → COALESCE 为 0
    Operation op = makeOp(base, EventType::DialogClose, InputMethod::Mouse, "MsgBox", false);
    op.windowTitle = "MsgBox";
    // duration 默认 nullopt
    Database::instance().insertOperation(op);

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT total_duration, avg_duration FROM agg_dialog_stats WHERE dialog_class='MsgBox'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
    QVERIFY(q.value(1).isNull() || q.value(1).toInt() == 0);
}

// ========== 时间分布 ==========

void TestAggregator::testTimeDistribution() {
    qint64 base = baseTime_.toMSecsSinceEpoch(); // 10:30
    Database::instance().batchInsert({
        makeOp(base),       // 10:30
        makeOp(base + 100), // 10:30
        makeOp(base + 200), // 10:30
    });

    Aggregator agg;
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT hour, count FROM agg_time_distribution WHERE date='2026-06-15'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 10);  // 10点
    QCOMPARE(q.value(1).toInt(), 3);
}

void TestAggregator::testTimeDistributionHourlyBuckets() {
    // 跨3个小时的数据
    qint64 h10 = QDateTime(QDate(2026, 6, 15), QTime(10, 0, 0)).toMSecsSinceEpoch();
    qint64 h11 = QDateTime(QDate(2026, 6, 15), QTime(11, 0, 0)).toMSecsSinceEpoch();
    qint64 h12 = QDateTime(QDate(2026, 6, 15), QTime(12, 0, 0)).toMSecsSinceEpoch();

    Database::instance().batchInsert({
        makeOp(h10 + 1000), makeOp(h10 + 2000),
        makeOp(h11 + 1000),
        makeOp(h12 + 1000), makeOp(h12 + 2000), makeOp(h12 + 3000),
    });

    Aggregator agg;
    agg.aggregateRange(QDateTime(QDate(2026, 6, 15), QTime(10, 0, 0)),
                       QDateTime(QDate(2026, 6, 15), QTime(13, 0, 0)),
                       Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT hour, count FROM agg_time_distribution WHERE date='2026-06-15' ORDER BY hour");
    QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 10); QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 11); QCOMPARE(q.value(1).toInt(), 1);
    QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 12); QCOMPARE(q.value(1).toInt(), 3);
}

// ========== 幂等性 ==========

void TestAggregator::testAggregateIdempotent() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().batchInsert({
        makeOp(base, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "click", 10),
        makeOp(base + 1, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "click", 10),
    });

    Aggregator agg;
    QDateTime start = baseTime_;
    QDateTime end = baseTime_.addSecs(3600);

    // 聚合两次
    agg.aggregateRange(start, end, Aggregator::Granularity::Hour);
    agg.aggregateRange(start, end, Aggregator::Granularity::Hour);

    // INSERT OR REPLACE → 不应该翻倍
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT count FROM agg_operation_stats WHERE action_key='click' AND granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2);  // 仍然是2，不是4
}

// ========== 信号 ==========

void TestAggregator::testAggregationCompletedSignal() {
    Aggregator agg;
    QSignalSpy spy(&agg, &Aggregator::aggregationCompleted);
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);
    QCOMPARE(spy.count(), 1);
}

// ========== 粒度格式 ==========

void TestAggregator::testGranularityFormat() {
    // Hour 粒度 → "yyyy-MM-dd HH:00"
    // Day 粒度 → "yyyy-MM-dd"
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().batchInsert({makeOp(base)});

    Aggregator agg;
    // Hour
    agg.aggregateRange(baseTime_, baseTime_.addSecs(3600), Aggregator::Granularity::Hour);
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT DISTINCT time_bucket FROM agg_input_stats WHERE granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("2026-06-15 10:00"));

    // 清掉重新测 Day
    q.exec("DELETE FROM agg_input_stats");
    agg.aggregateRange(baseTime_, baseTime_.addDays(1), Aggregator::Granularity::Day);
    q.exec("SELECT DISTINCT time_bucket FROM agg_input_stats WHERE granularity='day'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("2026-06-15"));
}

// ========== 跨天边界 ==========

void TestAggregator::testCrossDayHourBuckets() {
    // 数据跨越午夜：6/15 23:30 ~ 6/16 00:30
    QDateTime day1(QDate(2026, 6, 15), QTime(23, 30, 0));
    QDateTime day2(QDate(2026, 6, 16), QTime(0, 30, 0));
    qint64 ts1 = day1.toMSecsSinceEpoch();
    qint64 ts2 = day2.toMSecsSinceEpoch();

    Database::instance().batchInsert({
        makeOp(ts1),       // 6/15 23:30
        makeOp(ts1 + 60000),  // 6/15 23:31
        makeOp(ts2),       // 6/16 00:30
        makeOp(ts2 + 60000),  // 6/16 00:31
    });

    Aggregator agg;
    agg.aggregateRange(day1, day2.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    // 6/15 23点桶 → 2条
    q.exec("SELECT count FROM agg_input_stats WHERE time_bucket='2026-06-15 23:00' AND granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2);
    // 6/16 00点桶 → 2条
    q.exec("SELECT count FROM agg_input_stats WHERE time_bucket='2026-06-16 00:00' AND granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2);
}

void TestAggregator::testCrossDayDayBuckets() {
    // 3天数据：6/15 2条, 6/16 3条, 6/17 1条
    QDateTime d1(QDate(2026, 6, 15), QTime(10, 0, 0));
    QDateTime d2(QDate(2026, 6, 16), QTime(15, 0, 0));
    QDateTime d3(QDate(2026, 6, 17), QTime(8, 0, 0));

    Database::instance().batchInsert({
        makeOp(d1.toMSecsSinceEpoch()),
        makeOp(d1.addSecs(60).toMSecsSinceEpoch()),
        makeOp(d2.toMSecsSinceEpoch()),
        makeOp(d2.addSecs(60).toMSecsSinceEpoch()),
        makeOp(d2.addSecs(120).toMSecsSinceEpoch()),
        makeOp(d3.toMSecsSinceEpoch()),
    });

    Aggregator agg;
    agg.aggregateRange(d1, d3.addSecs(3600), Aggregator::Granularity::Day);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT time_bucket, count FROM agg_input_stats WHERE granularity='day' ORDER BY time_bucket");
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-15")); QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-16")); QCOMPARE(q.value(1).toInt(), 3);
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-17")); QCOMPARE(q.value(1).toInt(), 1);
}

void TestAggregator::testCrossDayTimeDistribution() {
    // 跨午夜的时间分布：23点和次日0点
    QDateTime late(QDate(2026, 6, 15), QTime(23, 45, 0));
    QDateTime early(QDate(2026, 6, 16), QTime(0, 15, 0));

    Database::instance().batchInsert({
        makeOp(late.toMSecsSinceEpoch()),
        makeOp(late.addSecs(60).toMSecsSinceEpoch()),
        makeOp(early.toMSecsSinceEpoch()),
        makeOp(early.addSecs(60).toMSecsSinceEpoch()),
        makeOp(early.addSecs(120).toMSecsSinceEpoch()),
    });

    Aggregator agg;
    agg.aggregateRange(late, early.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    // 6/15 23点 → 2条
    q.exec("SELECT count FROM agg_time_distribution WHERE date='2026-06-15' AND hour=23");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2);
    // 6/16 0点 → 3条
    q.exec("SELECT count FROM agg_time_distribution WHERE date='2026-06-16' AND hour=0");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 3);
}

void TestAggregator::testCrossDayMidnightExact() {
    // 恰好在午夜 00:00:00 的数据
    QDateTime midnight(QDate(2026, 6, 16), QTime(0, 0, 0));
    QDateTime justBefore(QDate(2026, 6, 15), QTime(23, 59, 59));

    Database::instance().batchInsert({
        makeOp(justBefore.toMSecsSinceEpoch()),
        makeOp(midnight.toMSecsSinceEpoch()),
        makeOp(midnight.addSecs(1).toMSecsSinceEpoch()),
    });

    Aggregator agg;
    agg.aggregateRange(justBefore, midnight.addSecs(60), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    // 6/15 23点 → 1条（23:59:59）
    q.exec("SELECT count FROM agg_input_stats WHERE time_bucket='2026-06-15 23:00' AND granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
    // 6/16 00点 → 2条（00:00:00 和 00:00:01）
    q.exec("SELECT count FROM agg_input_stats WHERE time_bucket='2026-06-16 00:00' AND granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2);
}

void TestAggregator::testCrossDayOperationsSameAction() {
    // 同一 action 跨3天，验证按天聚合各自独立
    QDateTime d1(QDate(2026, 6, 15), QTime(9, 0, 0));
    QDateTime d2(QDate(2026, 6, 16), QTime(9, 0, 0));
    QDateTime d3(QDate(2026, 6, 17), QTime(9, 0, 0));

    Database::instance().batchInsert({
        makeOp(d1.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "save"),
        makeOp(d1.addSecs(60).toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "save"),
        makeOp(d2.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "save"),
        makeOp(d3.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "save"),
        makeOp(d3.addSecs(60).toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "save"),
        makeOp(d3.addSecs(120).toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "save"),
    });

    Aggregator agg;
    agg.aggregateRange(d1, d3.addSecs(3600), Aggregator::Granularity::Day);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT time_bucket, count FROM agg_operation_stats WHERE action_key='save' AND granularity='day' ORDER BY time_bucket");
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-15")); QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-16")); QCOMPARE(q.value(1).toInt(), 1);
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-17")); QCOMPARE(q.value(1).toInt(), 3);
}

void TestAggregator::testCrossDayModuleConsistency() {
    // 同一模块跨天，验证每天独立计数
    QDateTime d1(QDate(2026, 6, 15), QTime(14, 0, 0));
    QDateTime d2(QDate(2026, 6, 16), QTime(14, 0, 0));

    Database::instance().batchInsert({
        makeOp(d1.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "SettingsDlg", false),
        makeOp(d1.addSecs(60).toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "SettingsDlg", false),
        makeOp(d2.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "SettingsDlg", false),
    });

    Aggregator agg;
    agg.aggregateRange(d1, d2.addSecs(3600), Aggregator::Granularity::Day);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT time_bucket, count FROM agg_module_stats WHERE module_class='SettingsDlg' AND granularity='day' ORDER BY time_bucket");
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-15")); QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-16")); QCOMPARE(q.value(1).toInt(), 1);
}

void TestAggregator::testCrossDayHeatmapSplit() {
    // 热力图跨天：同一 heat_region 在两天分别计数
    QDateTime d1(QDate(2026, 6, 15), QTime(10, 0, 0));
    QDateTime d2(QDate(2026, 6, 16), QTime(10, 0, 0));

    Database::instance().batchInsert({
        makeOp(d1.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "", 50),
        makeOp(d1.addSecs(60).toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "", 50),
        makeOp(d2.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "", 50),
    });

    Aggregator agg;
    agg.aggregateRange(d1, d2.addSecs(3600), Aggregator::Granularity::Day);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT time_bucket, count FROM agg_heatmap_stats WHERE heat_region=50 AND granularity='day' ORDER BY time_bucket");
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-15")); QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString("2026-06-16")); QCOMPARE(q.value(1).toInt(), 1);
}

void TestAggregator::testCrossDayDialogAcrossMidnight() {
    // dialog_close 跨午夜：23:59 关闭一个，00:01 关闭另一个
    QDateTime late(QDate(2026, 6, 15), QTime(23, 59, 0));
    QDateTime early(QDate(2026, 6, 16), QTime(0, 1, 0));

    Operation op1 = makeOp(late.toMSecsSinceEpoch(), EventType::DialogClose, InputMethod::Mouse, "MsgBox", false);
    op1.windowTitle = "MsgBox";
    op1.duration = 2000;
    Operation op2 = makeOp(early.toMSecsSinceEpoch(), EventType::DialogClose, InputMethod::Mouse, "MsgBox", false);
    op2.windowTitle = "MsgBox";
    op2.duration = 4000;

    Database::instance().batchInsert({op1, op2});

    Aggregator agg;
    agg.aggregateRange(late, early.addSecs(3600), Aggregator::Granularity::Hour);

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    // 6/15 23点桶
    q.exec("SELECT open_count, total_duration FROM agg_dialog_stats WHERE dialog_class='MsgBox' AND time_bucket='2026-06-15 23:00'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);       // 无 dialog_open
    QCOMPARE(q.value(1).toInt(), 2000);
    // 6/16 00点桶
    q.exec("SELECT open_count, total_duration FROM agg_dialog_stats WHERE dialog_class='MsgBox' AND time_bucket='2026-06-16 00:00'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);       // 无 dialog_open
    QCOMPARE(q.value(1).toInt(), 4000);
}

void TestAggregator::testCrossDayIdempotent() {
    // 跨天聚合两次，验证幂等
    QDateTime d1(QDate(2026, 6, 15), QTime(22, 0, 0));
    QDateTime d2(QDate(2026, 6, 16), QTime(2, 0, 0));

    Database::instance().batchInsert({
        makeOp(d1.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "click", 10),
        makeOp(d1.addSecs(3600).toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "click", 10),
        makeOp(d2.toMSecsSinceEpoch(), EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "click", 10),
    });

    Aggregator agg;
    // 聚合两次
    agg.aggregateRange(d1, d2.addSecs(3600), Aggregator::Granularity::Hour);
    agg.aggregateRange(d1, d2.addSecs(3600), Aggregator::Granularity::Hour);

    // 总数应仍为 3，不翻倍
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT SUM(count) FROM agg_input_stats WHERE granularity='hour'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 3);
}

QTEST_MAIN(TestAggregator)
#include "TestAggregator.moc"
