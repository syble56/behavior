// TestQueryFilter.cpp — 查询过滤器组合 + 边界条件
#include <gtest/gtest.h>
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

class TestQueryFilter : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;


protected:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
    qint64 base_ = 0;
};

void TestQueryFilter::SetUp() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/qf_test.db";
    Config::instance().setDatabasePath(path_);
    Database::instance().open(path_);
    base_ = QDateTime::currentMSecsSinceEpoch();
}

void TestQueryFilter::TearDown() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

TEST_F(TestQueryFilter, testCombinedFilters) {
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
    EXPECT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].sessionId, QString("s1"));
    EXPECT_EQ(ops[0].eventType, EventType::MouseClick);
    EXPECT_TRUE(ops[0].isMainWindow);
}

TEST_F(TestQueryFilter, testExactBoundary) {
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
    EXPECT_EQ(ops.size(), 1);
}

TEST_F(TestQueryFilter, testNoResults) {
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base_ + 1000);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base_ + 2000);
    f.limit = 100;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_TRUE(ops.isEmpty());
}

TEST_F(TestQueryFilter, testLimitZero) {
    Database::instance().batchInsert({makeOp(base_), makeOp(base_ + 1)});
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base_ - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base_ + 10);
    f.limit = 0;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 0);
}

TEST_F(TestQueryFilter, testLargeTimestamp) {
    // 远未来时间戳
    qint64 farTs = QDateTime(QDate(2099, 12, 31), QTime(23, 59, 59)).toMSecsSinceEpoch();
    Operation op = makeOp(farTs);
    EXPECT_TRUE(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(farTs - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(farTs + 1);
    f.limit = 10;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].timestamp, farTs);
}

TEST_F(TestQueryFilter, testSpecialCharsInSessionId) {
    QString weird = "s'es\"s\nID\t特殊字符";
    Operation op = makeOp(base_, weird);
    EXPECT_TRUE(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base_ - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base_ + 10);
    f.sessionId = weird;
    f.limit = 10;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].sessionId, weird);
}

