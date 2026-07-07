// TestQueryFilter.cpp — 查询过滤器组合 + 边界条件
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDateTime>

#include "storage/database.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

namespace {
Operation makeOp(qint64 ts, const QString& sess = "s1",
                 EventType et = EventType::MouseClick,
                 const QString& win = "W", bool main = true) {
    Operation op;
    op.sessionId = sess;
    op.timestamp = ts;
    op.eventType = et;
    op.inputMethod = InputMethod::Mouse;
    op.windowClass = win;
    op.isMainWindow = main;
    return op;
}
} // namespace

class TestQueryFilter : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void testCombinedFilters();
    void testExactBoundary();
    void testNoResults();
    void testLimitZero();
    void testLargeTimestamp();
    void testSpecialCharsInSessionId();

private:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
    qint64 base_ = 0;
};

void TestQueryFilter::init() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/qf_test.db";
    Config::instance().setDatabasePath(path_);
    Database::instance().open(path_);
    base_ = QDateTime::currentMSecsSinceEpoch();
}

void TestQueryFilter::cleanup() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

void TestQueryFilter::testCombinedFilters() {
    Database::instance().batchInsert({
        makeOp(base_, "s1", EventType::MouseClick, "Main", true),
        makeOp(base_ + 1, "s1", EventType::Shortcut, "Main", true),
        makeOp(base_ + 2, "s2", EventType::MouseClick, "Main", true),
        makeOp(base_ + 3, "s1", EventType::MouseClick, "Dialog", false),
    });

    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base_ - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base_ + 10);
    f.sessionId = "s1";
    f.eventType = "mouse_click";
    f.onlyMainWindow = true;
    f.limit = 100;

    auto ops = Database::instance().queryOperations(f);
    // 只有第一条满足所有条件
    QCOMPARE(ops.size(), 1);
    QCOMPARE(ops[0].sessionId, QString("s1"));
    QCOMPARE(ops[0].eventType, EventType::MouseClick);
    QVERIFY(ops[0].isMainWindow);
}

void TestQueryFilter::testExactBoundary() {
    // time >= start AND time <= end（注意是 <=）
    Database::instance().batchInsert({
        makeOp(base_),
        makeOp(base_ + 100),
    });

    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base_);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base_);
    f.limit = 100;
    auto ops = Database::instance().queryOperations(f);
    // time >= base_ AND time <= base_ → 1条
    QCOMPARE(ops.size(), 1);
}

void TestQueryFilter::testNoResults() {
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base_ + 1000);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base_ + 2000);
    f.limit = 100;
    auto ops = Database::instance().queryOperations(f);
    QVERIFY(ops.isEmpty());
}

void TestQueryFilter::testLimitZero() {
    Database::instance().batchInsert({makeOp(base_), makeOp(base_ + 1)});
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base_ - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base_ + 10);
    f.limit = 0;
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 0);
}

void TestQueryFilter::testLargeTimestamp() {
    // 远未来时间戳
    qint64 farTs = QDateTime(QDate(2099, 12, 31), QTime(23, 59, 59)).toMSecsSinceEpoch();
    Operation op = makeOp(farTs);
    QVERIFY(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(farTs - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(farTs + 1);
    f.limit = 10;
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 1);
    QCOMPARE(ops[0].timestamp, farTs);
}

void TestQueryFilter::testSpecialCharsInSessionId() {
    QString weird = "s'es\"s\nID\t特殊字符";
    Operation op = makeOp(base_, weird);
    QVERIFY(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base_ - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base_ + 10);
    f.sessionId = weird;
    f.limit = 10;
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 1);
    QCOMPARE(ops[0].sessionId, weird);
}

QTEST_MAIN(TestQueryFilter)
#include "TestQueryFilter.moc"
