// TestCoordBoundary.cpp — 坐标边界条件测试
// P1: (0,0) 过滤、负坐标、超大坐标、NULL 坐标、热力图区域边界
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

#include "storage/database.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

namespace {
Operation makeClick(qint64 ts, int x, int y) {
    Operation op;
    op.sessionId = "coord-test";
    op.timestamp = ts;
    op.eventType = EventType::MouseClick;
    op.inputMethod = InputMethod::Mouse;
    op.windowClass = "MainWindow";
    op.isMainWindow = true;
    op.screenX = x;
    op.screenY = y;
    op.heatRegion = 0;
    return op;
}

int countFiltered(qint64 start, qint64 end) {
    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT COUNT(*) FROM operations "
        "WHERE time >= ? AND time < ? "
        "AND event_type IN ('mouse_click','touch_tap','area_click') "
        "AND screen_x IS NOT NULL AND screen_y IS NOT NULL "
        "AND (screen_x != 0 OR screen_y != 0)");
    q.addBindValue(start); q.addBindValue(end); q.exec();
    return q.next() ? q.value(0).toInt() : -1;
}
} // namespace

class TestCoordBoundary : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // --- (0,0) filtering ---
    void testZeroZeroExcluded();
    void testOnlyZeroZero();
    void testZeroXNonZeroY();      // (0, 100) should be kept
    void testNonZeroXZeroY();      // (100, 0) should be kept

    // --- Negative coordinates ---
    void testNegativeX();
    void testNegativeY();
    void testBothNegative();

    // --- Large coordinates (multi-monitor) ---
    void testBeyond1920();
    void testMaxIntCoordinate();

    // --- Heat region boundary ---
    void testHeatRegionZero();
    void testHeatRegionMax();
    void testHeatRegionNegative();

    // --- NULL coordinates ---
    void testNullCoordinates();

private:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
    qint64 base_;
};

void TestCoordBoundary::init() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/coord_test.db";
    Config::instance().setDatabasePath(path_);
    Database::instance().open(path_);
    base_ = QDateTime(QDate(2026, 7, 13), QTime(10, 0, 0)).toMSecsSinceEpoch();
}

void TestCoordBoundary::cleanup() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

// ===== (0,0) filtering =====

void TestCoordBoundary::testZeroZeroExcluded() {
    Database::instance().insertOperation(makeClick(base_,       0, 0));
    Database::instance().insertOperation(makeClick(base_ + 100, 100, 200));
    Database::instance().insertOperation(makeClick(base_ + 200, 0, 0));

    QCOMPARE(countFiltered(base_, base_ + 10000), 1);
}

void TestCoordBoundary::testOnlyZeroZero() {
    Database::instance().insertOperation(makeClick(base_,       0, 0));
    Database::instance().insertOperation(makeClick(base_ + 100, 0, 0));

    QCOMPARE(countFiltered(base_, base_ + 10000), 0);
}

void TestCoordBoundary::testZeroXNonZeroY() {
    // (0, 100) is a valid click at left edge — should NOT be filtered
    Database::instance().insertOperation(makeClick(base_,       0, 100));
    Database::instance().insertOperation(makeClick(base_ + 100, 0, 0));

    QCOMPARE(countFiltered(base_, base_ + 10000), 1);
}

void TestCoordBoundary::testNonZeroXZeroY() {
    // (100, 0) is a valid click at top edge — should NOT be filtered
    Database::instance().insertOperation(makeClick(base_,       100, 0));
    Database::instance().insertOperation(makeClick(base_ + 100, 0, 0));

    QCOMPARE(countFiltered(base_, base_ + 10000), 1);
}

// ===== Negative coordinates =====

void TestCoordBoundary::testNegativeX() {
    // Unusual but possible with multi-monitor offset
    Database::instance().insertOperation(makeClick(base_,       -100, 200));
    Database::instance().insertOperation(makeClick(base_ + 100, 0, 0));

    // -100 != 0 so it passes the (screen_x != 0 OR screen_y != 0) filter
    QCOMPARE(countFiltered(base_, base_ + 10000), 1);
}

void TestCoordBoundary::testNegativeY() {
    Database::instance().insertOperation(makeClick(base_,       100, -200));

    QCOMPARE(countFiltered(base_, base_ + 10000), 1);
}

void TestCoordBoundary::testBothNegative() {
    Database::instance().insertOperation(makeClick(base_,       -500, -500));

    QCOMPARE(countFiltered(base_, base_ + 10000), 1);
}

// ===== Large coordinates =====

void TestCoordBoundary::testBeyond1920() {
    // Multi-monitor: second monitor at x=1920
    Database::instance().insertOperation(makeClick(base_,       2560, 1440));
    Database::instance().insertOperation(makeClick(base_ + 100, 3840, 1080));

    QCOMPARE(countFiltered(base_, base_ + 10000), 2);
}

void TestCoordBoundary::testMaxIntCoordinate() {
    Database::instance().insertOperation(makeClick(base_,       2147483647, 2147483647));

    QCOMPARE(countFiltered(base_, base_ + 10000), 1);
}

// ===== Heat region boundary =====

void TestCoordBoundary::testHeatRegionZero() {
    Operation op = makeClick(base_, 100, 100);
    op.heatRegion = 0;  // top-left cell
    QVERIFY(Database::instance().insertOperation(op));

    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT heat_region FROM operations WHERE screen_x = 100");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
}

void TestCoordBoundary::testHeatRegionMax() {
    Operation op = makeClick(base_, 100, 100);
    op.heatRegion = 99;  // bottom-right cell
    QVERIFY(Database::instance().insertOperation(op));

    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT heat_region FROM operations WHERE heat_region = 99");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 99);
}

void TestCoordBoundary::testHeatRegionNegative() {
    // -1 means invalid/no region
    Operation op = makeClick(base_, 100, 100);
    op.heatRegion = -1;
    QVERIFY(Database::instance().insertOperation(op));

    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT heat_region FROM operations WHERE heat_region = -1");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), -1);
}

// ===== NULL coordinates =====

void TestCoordBoundary::testNullCoordinates() {
    // Insert via raw SQL to get NULL screen_x/screen_y
    QSqlQuery q(Database::instance().connection());
    q.prepare("INSERT INTO operations (session_id, time, event_type, screen_x, screen_y) "
              "VALUES (?, ?, 'mouse_click', NULL, NULL)");
    q.addBindValue("coord-test");
    q.addBindValue(base_);
    QVERIFY(q.exec());

    // NULL coords should be filtered out by the IS NOT NULL check
    QCOMPARE(countFiltered(base_, base_ + 10000), 0);
}

QTEST_MAIN(TestCoordBoundary)
#include "TestCoordBoundary.moc"
