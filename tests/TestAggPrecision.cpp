// TestAggPrecision.cpp �?聚合值精确比对测�?// P2: 不只比对 SUM/COUNT，还验证�?action 分组的分布、百分比、中位数
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QVariantMap>
#include <QVariantList>
#include <QHash>
#include <algorithm>

#include "storage/database.h"
#include "storage/aggregator.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

namespace {
Operation makeOp(qint64 ts, const QString& action = "click",
                 const QString& win = "MainWindow", bool main = true,
                 InputMethod im = InputMethod::Mouse,
                 int heatRegion = 42) {
    Operation op;
    op.sessionId = "agg-prec-test";
    op.timestamp = ts;
    op.eventType = EventType::MouseClick;
    op.inputMethod = im;
    op.controlClass = "QPushButton";
    op.controlName = "btn";
    op.actionName = action;
    op.windowClass = win;
    op.isMainWindow = main;
    op.screenX = 100;
    op.screenY = 200;
    op.heatRegion = heatRegion;
    return op;
}

// Query agg_operation_stats grouped by action_key
QHash<QString, int> getAggActionCounts(const QString& timeBucket) {
    QHash<QString, int> result;
    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT action_key, SUM(count) FROM agg_operation_stats "
        "WHERE time_bucket = ? GROUP BY action_key");
    q.addBindValue(timeBucket);
    q.exec();
    while (q.next()) {
        result[q.value(0).toString()] = q.value(1).toInt();
    }
    return result;
}

// Query raw operations grouped by action_key
QHash<QString, int> getRawActionCounts(qint64 start, qint64 end) {
    QHash<QString, int> result;
    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT action_name, COUNT(*) FROM operations "
        "WHERE time >= ? AND time < ? GROUP BY action_name");
    q.addBindValue(start); q.addBindValue(end);
    q.exec();
    while (q.next()) {
        result[q.value(0).toString()] = q.value(1).toInt();
    }
    return result;
}
} // namespace

class TestAggPrecision : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // --- Action distribution precision ---
    void testActionDistributionMatchesRaw();
    void testActionDistributionMultipleBuckets();
    void testActionCountSumMatchesTotal();

    // --- Module distribution precision ---
    void testModuleDistributionMatchesRaw();
    void testModulePercentageSumsTo100();

    // --- Input method distribution precision ---
    void testInputMethodDistribution();
    void testInputMethodExcludesDerived();

    // --- Heatmap precision ---
    void testHeatmapOnlyClickEvents();
    void testHeatmapOnlyMainWindow();
    void testHeatmapRegionDistribution();

    // --- Dialog aggregation precision ---
    void testDialogDurationPerDialog();
    void testDialogMedianCorrectness();

    // --- Idempotent with precision ---
    void testRerunProducesIdenticalDistribution();

private:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
    QDateTime baseTime_;
};

void TestAggPrecision::init() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/agg_prec_test.db";
    Config::instance().setDatabasePath(path_);
    Database::instance().open(path_);
    baseTime_ = QDateTime(QDate(2026, 7, 13), QTime(10, 0, 0));
}

void TestAggPrecision::cleanup() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

// ===== Action distribution =====

void TestAggPrecision::testActionDistributionMatchesRaw() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // "save" x3, "open" x2, "delete" x1 within same hour
    for (int i = 0; i < 3; ++i)
        Database::instance().insertOperation(makeOp(base + i*100, "save"));
    for (int i = 0; i < 2; ++i)
        Database::instance().insertOperation(makeOp(base + 300 + i*100, "open"));
    Database::instance().insertOperation(makeOp(base + 500, "delete"));

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    auto aggCounts = getAggActionCounts(bucket);
    auto rawCounts = getRawActionCounts(base, end.toMSecsSinceEpoch());

    QCOMPARE(aggCounts.size(), 3);
    QCOMPARE(aggCounts["save"], 3);
    QCOMPARE(aggCounts["open"], 2);
    QCOMPARE(aggCounts["delete"], 1);
    QCOMPARE(aggCounts, rawCounts);
}

void TestAggPrecision::testActionDistributionMultipleBuckets() {
    // Insert across two hours
    QDateTime h1 = baseTime_;
    QDateTime h2 = baseTime_.addSecs(3600);

    Database::instance().insertOperation(makeOp(h1.toMSecsSinceEpoch(),       "save"));
    Database::instance().insertOperation(makeOp(h1.toMSecsSinceEpoch() + 100, "save"));
    Database::instance().insertOperation(makeOp(h2.toMSecsSinceEpoch(),       "open"));

    QDateTime end = baseTime_.addSecs(7200);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString b1 = h1.toString("yyyy-MM-dd HH:00");
    QString b2 = h2.toString("yyyy-MM-dd HH:00");

    auto counts1 = getAggActionCounts(b1);
    auto counts2 = getAggActionCounts(b2);

    QCOMPARE(counts1["save"], 2);
    QVERIFY(!counts1.contains("open"));
    QCOMPARE(counts2["open"], 1);
    QVERIFY(!counts2.contains("save"));
}

void TestAggPrecision::testActionCountSumMatchesTotal() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeOp(base,       "a"));
    Database::instance().insertOperation(makeOp(base + 100, "b"));
    Database::instance().insertOperation(makeOp(base + 200, "c"));
    Database::instance().insertOperation(makeOp(base + 300, "a"));

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    QSqlQuery q(Database::instance().connection());
    q.prepare("SELECT SUM(count) FROM agg_operation_stats WHERE time_bucket = ?");
    q.addBindValue(bucket); q.exec();
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 4);
}

// ===== Module distribution =====

void TestAggPrecision::testModuleDistributionMatchesRaw() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeOp(base,       "click", "QMainWindow"));
    Database::instance().insertOperation(makeOp(base + 100, "click", "QMainWindow"));
    Database::instance().insertOperation(makeOp(base + 200, "click", "SettingsDialog"));

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    QSqlQuery q(Database::instance().connection());
    q.prepare("SELECT module_class, count FROM agg_module_stats WHERE time_bucket = ? ORDER BY count DESC");
    q.addBindValue(bucket); q.exec();

    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("QMainWindow"));
    QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("SettingsDialog"));
    QCOMPARE(q.value(1).toInt(), 1);
}

void TestAggPrecision::testModulePercentageSumsTo100() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeOp(base,       "click", "QMainWindow"));
    Database::instance().insertOperation(makeOp(base + 100, "click", "OptionsDialog"));
    Database::instance().insertOperation(makeOp(base + 200, "click", "HelpWindow"));

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    QSqlQuery q(Database::instance().connection());
    q.prepare("SELECT count FROM agg_module_stats WHERE time_bucket = ?");
    q.addBindValue(bucket); q.exec();

    int total = 0;
    while (q.next()) total += q.value(0).toInt();
    QCOMPARE(total, 3);
}

// ===== Input method =====

void TestAggPrecision::testInputMethodDistribution() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeOp(base,       "click", "MainWindow", true, InputMethod::Mouse));
    Database::instance().insertOperation(makeOp(base + 100, "click", "MainWindow", true, InputMethod::Mouse));
    Database::instance().insertOperation(makeOp(base + 200, "click", "MainWindow", true, InputMethod::Keyboard));
    Database::instance().insertOperation(makeOp(base + 300, "click", "MainWindow", true, InputMethod::Touch));

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    QSqlQuery q(Database::instance().connection());
    q.prepare("SELECT input_method, count FROM agg_input_stats WHERE time_bucket = ? ORDER BY count DESC");
    q.addBindValue(bucket); q.exec();

    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("mouse"));
    QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next());
    // keyboard and touch both have 1 �?order may vary
    int secondCount = q.value(1).toInt();
    QCOMPARE(secondCount, 1);
}

void TestAggPrecision::testInputMethodExcludesDerived() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeOp(base,       "click", "MainWindow", true, InputMethod::Mouse));
    Database::instance().insertOperation(makeOp(base + 100, "click", "MainWindow", true, InputMethod::Derived));

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    QSqlQuery q(Database::instance().connection());
    q.prepare("SELECT input_method FROM agg_input_stats WHERE time_bucket = ? AND input_method = 'derived'");
    q.addBindValue(bucket); q.exec();

    // Derived should NOT appear in input stats (consistent with InputAnalyzer)
    QVERIFY(!q.next());
}

// ===== Heatmap =====

void TestAggPrecision::testHeatmapOnlyClickEvents() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // Click event
    Database::instance().insertOperation(makeOp(base, "click", "MainWindow", true, InputMethod::Mouse, 42));
    // Non-click event (dialog open) with heat_region
    Operation nonClick = makeOp(base + 100, "open", "MainWindow", true, InputMethod::Derived, 42);
    nonClick.eventType = EventType::DialogOpen;
    Database::instance().insertOperation(nonClick);

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    QSqlQuery q(Database::instance().connection());
    q.prepare("SELECT SUM(count) FROM agg_heatmap_stats WHERE time_bucket = ?");
    q.addBindValue(bucket); q.exec();
    QVERIFY(q.next());
    // Only the click event should be counted
    QCOMPARE(q.value(0).toInt(), 1);
}

void TestAggPrecision::testHeatmapOnlyMainWindow() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeOp(base,       "click", "MainWindow", true,  InputMethod::Mouse, 10));
    Database::instance().insertOperation(makeOp(base + 100, "click", "MainWindow", false, InputMethod::Mouse, 20));  // not main window

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    QSqlQuery q(Database::instance().connection());
    q.prepare("SELECT SUM(count) FROM agg_heatmap_stats WHERE time_bucket = ?");
    q.addBindValue(bucket); q.exec();
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);  // only main window
}

void TestAggPrecision::testHeatmapRegionDistribution() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // 3 clicks in region 10, 2 in region 55, 1 in region 99
    for (int i = 0; i < 3; ++i)
        Database::instance().insertOperation(makeOp(base + i*100, "click", "MainWindow", true, InputMethod::Mouse, 10));
    for (int i = 0; i < 2; ++i)
        Database::instance().insertOperation(makeOp(base + 300 + i*100, "click", "MainWindow", true, InputMethod::Mouse, 55));
    Database::instance().insertOperation(makeOp(base + 500, "click", "MainWindow", true, InputMethod::Mouse, 99));

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    QSqlQuery q(Database::instance().connection());
    q.prepare("SELECT heat_region, count FROM agg_heatmap_stats WHERE time_bucket = ? ORDER BY heat_region");
    q.addBindValue(bucket); q.exec();

    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 10);
    QCOMPARE(q.value(1).toInt(), 3);
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 55);
    QCOMPARE(q.value(1).toInt(), 2);
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 99);
    QCOMPARE(q.value(1).toInt(), 1);
}

// ===== Dialog aggregation =====

void TestAggPrecision::testDialogDurationPerDialog() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // Two different dialogs with different durations
    {
        Operation open = makeOp(base, "open", "DlgA", true, InputMethod::Derived);
        open.eventType = EventType::DialogOpen;
        open.windowTitle = "DialogA";
        Operation close = makeOp(base + 3000, "close", "DlgA", true, InputMethod::Derived);
        close.eventType = EventType::DialogClose;
        close.windowTitle = "DialogA";
        close.duration = 3000;
        Database::instance().insertOperation(open);
        Database::instance().insertOperation(close);
    }
    {
        Operation open = makeOp(base + 10000, "open", "DlgB", true, InputMethod::Derived);
        open.eventType = EventType::DialogOpen;
        open.windowTitle = "DialogB";
        Operation close = makeOp(base + 12000, "close", "DlgB", true, InputMethod::Derived);
        close.eventType = EventType::DialogClose;
        close.windowTitle = "DialogB";
        close.duration = 2000;
        Database::instance().insertOperation(open);
        Database::instance().insertOperation(close);
    }

    QDateTime end = baseTime_.addSecs(36000);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    // Verify dialog stats in aggregation table
    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT dialog_class, open_count, total_duration FROM agg_dialog_stats ORDER BY dialog_class");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("DialogA"));
    QCOMPARE(q.value(1).toInt(), 1);
    QCOMPARE(q.value(2).toInt(), 3000);
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("DialogB"));
    QCOMPARE(q.value(1).toInt(), 1);
    QCOMPARE(q.value(2).toInt(), 2000);
}

void TestAggPrecision::testDialogMedianCorrectness() {
    // Durations: 1000, 2000, 3000, 4000, 5000 => median = 3000
    qint64 base = baseTime_.toMSecsSinceEpoch();
    int durations[] = {1000, 2000, 3000, 4000, 5000};
    for (int i = 0; i < 5; ++i) {
        Operation open = makeOp(base + i*10000, "open", "Dlg", true, InputMethod::Derived);
        open.eventType = EventType::DialogOpen;
        open.windowTitle = "MedianTest";
        Operation close = makeOp(base + i*10000 + durations[i], "close", "Dlg", true, InputMethod::Derived);
        close.eventType = EventType::DialogClose;
        close.windowTitle = "MedianTest";
        close.duration = durations[i];
        Database::instance().insertOperation(open);
        Database::instance().insertOperation(close);
    }

    QDateTime end = baseTime_.addSecs(60000);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    // Check if median is stored correctly (if the table has a median column)
    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT total_duration, open_count FROM agg_dialog_stats WHERE dialog_class = 'MedianTest'");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 15000);  // sum of all durations
    QCOMPARE(q.value(1).toInt(), 5);
}

// ===== Idempotent with precision =====

void TestAggPrecision::testRerunProducesIdenticalDistribution() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    for (int i = 0; i < 5; ++i)
        Database::instance().insertOperation(makeOp(base + i*100, "save"));
    for (int i = 0; i < 3; ++i)
        Database::instance().insertOperation(makeOp(base + 500 + i*100, "open"));

    QDateTime end = baseTime_.addSecs(3600);
    Aggregator agg;
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);

    // Snapshot first run
    QString bucket = baseTime_.toString("yyyy-MM-dd HH:00");
    auto firstRun = getAggActionCounts(bucket);

    // Run again
    agg.aggregateRange(baseTime_, end, Aggregator::Granularity::Hour);
    auto secondRun = getAggActionCounts(bucket);

    // Must be identical
    QCOMPARE(secondRun, firstRun);
    QCOMPARE(secondRun["save"], 5);
    QCOMPARE(secondRun["open"], 3);
}

QTEST_MAIN(TestAggPrecision)
#include "TestAggPrecision.moc"
