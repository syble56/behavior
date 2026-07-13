// TestDistanceCalc.cpp — 距离计算正确性验证
// P0: 验证 sqrt(dx^2 + dy^2) 的逐点距离、日均值、总量
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <cmath>

#include "storage/database.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

namespace {
Operation makeClick(qint64 ts, int x, int y, const QString& sess = "dist-test") {
    Operation op;
    op.sessionId = sess;
    op.timestamp = ts;
    op.eventType = EventType::MouseClick;
    op.inputMethod = InputMethod::Mouse;
    op.windowClass = "MainWindow";
    op.isMainWindow = true;
    op.screenX = x;
    op.screenY = y;
    op.heatRegion = 42;
    return op;
}

// Replicate the distance calculation logic from DistanceTab
double computeTotalDistance(const QList<QPair<int,int>>& pts) {
    if (pts.size() < 2) return 0.0;
    double total = 0;
    for (int i = 1; i < pts.size(); ++i) {
        double dx = pts[i].first - pts[i-1].first;
        double dy = pts[i].second - pts[i-1].second;
        total += std::sqrt(dx * dx + dy * dy);
    }
    return total;
}

double computeAvgDistance(const QList<QPair<int,int>>& pts) {
    if (pts.size() < 2) return 0.0;
    return computeTotalDistance(pts) / (pts.size() - 1);
}
} // namespace

class TestDistanceCalc : public QObject {
    Q_OBJECT
private slots:
    // --- Pure math tests (no DB needed) ---
    void testPythagoreanTriple_3_4();
    void testHorizontalOnly();
    void testVerticalOnly();
    void testSinglePoint();
    void testEmptyPoints();
    void testSamePoint();
    void testKnownTrajectory();
    void testAvgMatchesTotalDivN();
    void testMaxSingleStep();

    // --- DB-backed tests: verify query filters (0,0) ---
    void testFilterZeroZero();
    void testNegativeCoordinates();
    void testLargeCoordinates();
    void testMixedValidInvalid();
};

// ===== Pure math =====

void TestDistanceCalc::testPythagoreanTriple_3_4() {
    // (0,0) -> (3,4) => distance = 5
    QList<QPair<int,int>> pts = {{0,0}, {3,4}};
    QCOMPARE(computeTotalDistance(pts), 5.0);
}

void TestDistanceCalc::testHorizontalOnly() {
    // (0,0) -> (10,0) -> (20,0) => total = 20, avg = 10
    QList<QPair<int,int>> pts = {{0,0}, {10,0}, {20,0}};
    QCOMPARE(computeTotalDistance(pts), 20.0);
    QCOMPARE(computeAvgDistance(pts), 10.0);
}

void TestDistanceCalc::testVerticalOnly() {
    // (5,0) -> (5,8) -> (5,8) => total = 8, avg = 4
    QList<QPair<int,int>> pts = {{5,0}, {5,8}, {5,8}};
    QCOMPARE(computeTotalDistance(pts), 8.0);
    QCOMPARE(computeAvgDistance(pts), 4.0);
}

void TestDistanceCalc::testSinglePoint() {
    QList<QPair<int,int>> pts = {{100, 200}};
    QCOMPARE(computeTotalDistance(pts), 0.0);
    QCOMPARE(computeAvgDistance(pts), 0.0);
}

void TestDistanceCalc::testEmptyPoints() {
    QList<QPair<int,int>> pts;
    QCOMPARE(computeTotalDistance(pts), 0.0);
    QCOMPARE(computeAvgDistance(pts), 0.0);
}

void TestDistanceCalc::testSamePoint() {
    // (50,50) -> (50,50) -> (50,50) => all zero
    QList<QPair<int,int>> pts = {{50,50}, {50,50}, {50,50}};
    QCOMPARE(computeTotalDistance(pts), 0.0);
    QCOMPARE(computeAvgDistance(pts), 0.0);
}

void TestDistanceCalc::testKnownTrajectory() {
    // (0,0) -> (3,4) -> (3,8) => 5 + 4 = 9, avg = 4.5
    QList<QPair<int,int>> pts = {{0,0}, {3,4}, {3,8}};
    QCOMPARE(computeTotalDistance(pts), 9.0);
    QCOMPARE(computeAvgDistance(pts), 4.5);
}

void TestDistanceCalc::testAvgMatchesTotalDivN() {
    // Random-ish trajectory
    QList<QPair<int,int>> pts = {{10,20}, {100,50}, {80,200}, {300,10}, {250,250}};
    double total = computeTotalDistance(pts);
    double avg = computeAvgDistance(pts);
    QVERIFY(avg > 0);
    QCOMPARE(avg, total / (pts.size() - 1));
}

void TestDistanceCalc::testMaxSingleStep() {
    // Diagonal across 1920x1080 screen
    QList<QPair<int,int>> pts = {{0,0}, {1920,1080}};
    double expected = std::sqrt(1920.0*1920.0 + 1080.0*1080.0);
    QCOMPARE(computeTotalDistance(pts), expected);
}

// ===== DB-backed: (0,0) filtering and coordinate edge cases =====

void TestDistanceCalc::testFilterZeroZero() {
    QTemporaryDir dir;
    QString path = dir.path() + "/dist_test.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);

    qint64 base = QDateTime(QDate(2026,7,13), QTime(10,0,0)).toMSecsSinceEpoch();
    // Insert: valid, (0,0), valid
    Database::instance().insertOperation(makeClick(base,       100, 200));
    Database::instance().insertOperation(makeClick(base + 100, 0,   0));    // should be filtered
    Database::instance().insertOperation(makeClick(base + 200, 300, 400));

    // Query with the same filter as DistanceTab uses
    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT screen_x, screen_y FROM operations "
        "WHERE time >= ? AND time < ? "
        "AND event_type IN ('mouse_click','touch_tap','area_click') "
        "AND screen_x IS NOT NULL AND screen_y IS NOT NULL "
        "AND (screen_x != 0 OR screen_y != 0) "
        "ORDER BY time");
    qint64 end = base + 10000;
    q.addBindValue(base); q.addBindValue(end); q.exec();

    QList<QPair<int,int>> pts;
    while (q.next()) {
        pts.append({q.value(0).toInt(), q.value(1).toInt()});
    }

    QCOMPARE(pts.size(), 2);
    QCOMPARE(pts[0].first, 100);
    QCOMPARE(pts[0].second, 200);
    QCOMPARE(pts[1].first, 300);
    QCOMPARE(pts[1].second, 400);

    Database::instance().close();
}

void TestDistanceCalc::testNegativeCoordinates() {
    QTemporaryDir dir;
    QString path = dir.path() + "/dist_neg.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);

    qint64 base = QDateTime(QDate(2026,7,13), QTime(10,0,0)).toMSecsSinceEpoch();
    // Negative coords (shouldn't happen in practice, but test the math)
    Database::instance().insertOperation(makeClick(base,       -10, -20));
    Database::instance().insertOperation(makeClick(base + 100, -13, -24));

    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT screen_x, screen_y FROM operations "
        "WHERE time >= ? AND time < ? "
        "AND (screen_x != 0 OR screen_y != 0) ORDER BY time");
    qint64 end = base + 10000;
    q.addBindValue(base); q.addBindValue(end); q.exec();

    QList<QPair<int,int>> pts;
    while (q.next()) pts.append({q.value(0).toInt(), q.value(1).toInt()});

    QCOMPARE(pts.size(), 2);
    // Distance from (-10,-20) to (-13,-24) = sqrt(9+16) = 5
    QCOMPARE(computeTotalDistance(pts), 5.0);

    Database::instance().close();
}

void TestDistanceCalc::testLargeCoordinates() {
    QTemporaryDir dir;
    QString path = dir.path() + "/dist_large.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);

    qint64 base = QDateTime(QDate(2026,7,13), QTime(10,0,0)).toMSecsSinceEpoch();
    // Multi-monitor scenario: coords beyond 1920
    Database::instance().insertOperation(makeClick(base,       3000, 500));
    Database::instance().insertOperation(makeClick(base + 100, 4000, 500));

    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT screen_x, screen_y FROM operations "
        "WHERE time >= ? AND time < ? "
        "AND (screen_x != 0 OR screen_y != 0) ORDER BY time");
    qint64 end = base + 10000;
    q.addBindValue(base); q.addBindValue(end); q.exec();

    QList<QPair<int,int>> pts;
    while (q.next()) pts.append({q.value(0).toInt(), q.value(1).toInt()});

    QCOMPARE(pts.size(), 2);
    QCOMPARE(computeTotalDistance(pts), 1000.0);  // pure horizontal

    Database::instance().close();
}

void TestDistanceCalc::testMixedValidInvalid() {
    QTemporaryDir dir;
    QString path = dir.path() + "/dist_mixed.db";
    Config::instance().setDatabasePath(path);
    Database::instance().open(path);

    qint64 base = QDateTime(QDate(2026,7,13), QTime(10,0,0)).toMSecsSinceEpoch();
    // Mix: valid, (0,0), valid, (0,0), (0,0), valid
    Database::instance().insertOperation(makeClick(base,         10, 10));
    Database::instance().insertOperation(makeClick(base + 100,   0,  0));
    Database::instance().insertOperation(makeClick(base + 200,  20, 10));
    Database::instance().insertOperation(makeClick(base + 300,   0,  0));
    Database::instance().insertOperation(makeClick(base + 400,   0,  0));
    Database::instance().insertOperation(makeClick(base + 500,  30, 10));

    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT screen_x, screen_y FROM operations "
        "WHERE time >= ? AND time < ? "
        "AND (screen_x != 0 OR screen_y != 0) ORDER BY time");
    qint64 end = base + 10000;
    q.addBindValue(base); q.addBindValue(end); q.exec();

    QList<QPair<int,int>> pts;
    while (q.next()) pts.append({q.value(0).toInt(), q.value(1).toInt()});

    // Only 3 valid points
    QCOMPARE(pts.size(), 3);
    // (10,10) -> (20,10) -> (30,10) => 10 + 10 = 20
    QCOMPARE(computeTotalDistance(pts), 20.0);

    Database::instance().close();
}

QTEST_MAIN(TestDistanceCalc)
#include "TestDistanceCalc.moc"
