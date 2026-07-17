// TestTimezone.cpp — 时区处理自动化测试
// P1: 验证 strftime('localtime') vs QDateTime 本地时间一致性
#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QTimeZone>

#include "storage/database.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

namespace {
Operation makeClick(qint64 ts) {
    Operation op;
    op.sessionId = "tz-test";
    op.timestamp = ts;
    op.eventType = EventType::MouseClick;
    op.inputMethod = InputMethod::Mouse;
    op.windowClass = "MainWindow";
    op.isMainWindow = true;
    op.screenX = 100;
    op.screenY = 200;
    return op;
}
} // namespace

class TestTimezone : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // --- Core: stored timestamp is UTC ms, query converts to local ---

    // --- Edge cases ---

protected:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
};

void TestTimezone::SetUp() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/tz_test.db";
    Config::instance().setDatabasePath(path_);
    Database::instance().open(path_);
}

void TestTimezone::TearDown() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

TEST_F(TestTimezone, testStoredAsUtcMs) {
    // Insert an operation at a known local time
    QDateTime local(QDate(2026, 7, 13), QTime(15, 30, 0));  // local time
    qint64 ms = local.toMSecsSinceEpoch();  // this is UTC ms

    Database::instance().insertOperation(makeClick(ms));

    // Read back the raw timestamp
    QSqlQuery q(Database::instance().connection());
    q.exec("SELECT time FROM operations");
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toLongLong(), ms);
}

TEST_F(TestTimezone, testStrftimeLocaltimeMatchesQDateTime) {
    // Insert at 2026-07-13 15:30:00 local
    QDateTime local(QDate(2026, 7, 13), QTime(15, 30, 0));
    qint64 ms = local.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeClick(ms));

    // Query the date string using the same SQL as DistanceTab
    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) as dt, "
        "strftime('%H', datetime(time/1000,'unixepoch','localtime')) as hr "
        "FROM operations WHERE time = ?");
    q.addBindValue(ms);
    q.exec();

    EXPECT_TRUE(q.next());
    QString dtStr = q.value(0).toString();
    QString hrStr = q.value(1).toString();

    // The SQL localtime should match QDateTime local time
    EXPECT_EQ(dtStr, QString("2026-07-13"));
    EXPECT_EQ(hrStr, QString("15"));
}

TEST_F(TestTimezone, testDateBoundaryLocalTime) {
    // Insert at 2026-07-13 23:59:59 local — should be 07-13, not 07-14
    QDateTime local(QDate(2026, 7, 13), QTime(23, 59, 59));
    qint64 ms = local.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeClick(ms));

    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) "
        "FROM operations WHERE time = ?");
    q.addBindValue(ms);
    q.exec();
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toString(), QString("2026-07-13"));
}

TEST_F(TestTimezone, testCrossDayAtMidnightLocal) {
    // 23:59:59 and 00:00:01 next day
    QDateTime day1(QDate(2026, 7, 13), QTime(23, 59, 59));
    QDateTime day2(QDate(2026, 7, 14), QTime(0, 0, 1));

    Database::instance().insertOperation(makeClick(day1.toMSecsSinceEpoch()));
    Database::instance().insertOperation(makeClick(day2.toMSecsSinceEpoch()));

    QSqlQuery q(Database::instance().connection());
    q.exec(
        "SELECT strftime('%Y-%m-%d', datetime(time/1000,'unixepoch','localtime')) "
        "FROM operations ORDER BY time");

    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toString(), QString("2026-07-13"));
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toString(), QString("2026-07-14"));
}

TEST_F(TestTimezone, testHourBucketLocalTime) {
    // Insert at various hours and verify hour buckets
    int hours[] = {0, 6, 12, 18, 23};
    for (int h : hours) {
        QDateTime local(QDate(2026, 7, 13), QTime(h, 30, 0));
        Database::instance().insertOperation(makeClick(local.toMSecsSinceEpoch()));
    }

    QSqlQuery q(Database::instance().connection());
    q.exec(
        "SELECT strftime('%H', datetime(time/1000,'unixepoch','localtime')) "
        "FROM operations ORDER BY time");

    QStringList expected = {"00", "06", "12", "18", "23"};
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(q.next());
        EXPECT_EQ(q.value(0).toString(), expected[i]);
    }
}

TEST_F(TestTimezone, testNoonLocalTime) {
    QDateTime local(QDate(2026, 7, 13), QTime(12, 0, 0));
    qint64 ms = local.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeClick(ms));

    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT strftime('%H:%M', datetime(time/1000,'unixepoch','localtime')) "
        "FROM operations WHERE time = ?");
    q.addBindValue(ms);
    q.exec();
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toString(), QString("12:00"));
}

TEST_F(TestTimezone, testEndOfDayLocalTime) {
    QDateTime local(QDate(2026, 7, 13), QTime(23, 59, 59, 999));
    qint64 ms = local.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeClick(ms));

    QSqlQuery q(Database::instance().connection());
    q.prepare(
        "SELECT strftime('%H:%M:%S', datetime(time/1000,'unixepoch','localtime')) "
        "FROM operations WHERE time = ?");
    q.addBindValue(ms);
    q.exec();
    EXPECT_TRUE(q.next());
    // strftime truncates to seconds, so 23:59:59
    EXPECT_EQ(q.value(0).toString(), QString("23:59:59"));
}

