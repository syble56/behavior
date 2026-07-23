// TestDatabase.cpp — 数据库 CRUD + Schema + 线程连接
#include <gtest/gtest.h>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QDateTime>
#include <QThread>
#include <thread>

#include "storage/database.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

// ========== 辅助函数 ==========
namespace {
Operation makeOp(qint64 ts, EventType et = EventType::MouseClick,
                 const QString& win = "MainWindow", bool mainWin = true) {
    Operation op;
    op.sessionId = "test-session";
    op.timestamp = ts;
    op.eventType = et;
    op.inputMethod = InputMethod::Mouse;
    op.controlClass = "QPushButton";
    op.controlName = "btnOk";
    op.controlText = "OK";
    op.windowClass = win;
    op.windowTitle = "Test";
    op.isMainWindow = mainWin;
    op.screenX = 100;
    op.screenY = 200;
    op.heatRegion = 42;
    return op;
}

QString tempDbPath(QTemporaryDir& dir) {
    return dir.path() + "/test_behavior.db";
}
} // namespace

class TestDatabase : public ::testing::Test {
protected:
    // ===== 生命周期 =====
    void SetUp() override;
    void TearDown() override;

    // ===== Schema =====

    // ===== 插入 =====

    // ===== 查询 =====

    // ===== 会话 =====

    // ===== 清理 =====

    // ===== 导出 =====

    // ===== 数据完整性 =====

    // ===== 线程安全 =====

protected:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
};

void TestDatabase::SetUp() {
    dir_ = new QTemporaryDir;
    path_ = tempDbPath(*dir_);
    Config::instance().setDatabasePath(path_);
    EXPECT_TRUE(Database::instance().open(path_));
}

void TestDatabase::TearDown() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

// ========== Schema ==========

TEST_F(TestDatabase, testOpenCreatesAllTables) {
    QSqlDatabase db = Database::instance().connection();
    QStringList expected = {
        "metadata", "sessions", "operations",
        "agg_operation_stats", "agg_module_stats", "agg_input_stats",
        "agg_heatmap_stats", "agg_dialog_stats", "agg_time_distribution"
    };
    QSqlQuery q(db);
    EXPECT_TRUE(q.exec("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"));
    QStringList actual;
    while (q.next()) actual << q.value(0).toString();
    for (const auto& t : expected) {
        EXPECT_TRUE(actual.contains(t)) << ("Missing table: " + t).toStdString();
    }
}

TEST_F(TestDatabase, testOpenCreatesIndexes) {
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    EXPECT_TRUE(q.exec("SELECT name FROM sqlite_master WHERE type='index' ORDER BY name"));
    QStringList indexes;
    while (q.next()) indexes << q.value(0).toString();
    EXPECT_TRUE(indexes.contains("idx_ops_time"));
    EXPECT_TRUE(indexes.contains("idx_ops_session"));
    EXPECT_TRUE(indexes.contains("idx_ops_event"));
    EXPECT_TRUE(indexes.contains("idx_agg_ops_time"));
}

TEST_F(TestDatabase, testReopenSamePath) {
    Database::instance().close();
    EXPECT_TRUE(Database::instance().open(path_));
    EXPECT_TRUE(Database::instance().isOpen());
    // 数据应该还在
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    EXPECT_TRUE(q.exec("SELECT COUNT(*) FROM operations"));
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toLongLong(), qint64(0)); // 没插过数据
}

// ========== 插入 ==========

TEST_F(TestDatabase, testInsertSingleOperation) {
    Operation op = makeOp(QDateTime::currentMSecsSinceEpoch());
    EXPECT_TRUE(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addSecs(-60);
    f.endTime = QDateTime::currentDateTime().addSecs(60);
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].controlClass, QString("QPushButton"));
}

TEST_F(TestDatabase, testBatchInsert) {
    QList<Operation> batch;
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < 50; ++i) {
        batch.append(makeOp(base + i));
    }
    EXPECT_TRUE(Database::instance().batchInsert(batch));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 100);
    f.limit = 100;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 50);
}

TEST_F(TestDatabase, testBatchInsertEmpty) {
    EXPECT_TRUE(Database::instance().batchInsert({}));
    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addSecs(-60);
    f.endTime = QDateTime::currentDateTime().addSecs(60);
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 0);
}

TEST_F(TestDatabase, testInsertAllEventTypes) {
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    QList<Operation> ops;
    ops << makeOp(ts, EventType::MouseClick)
        << makeOp(ts + 1, EventType::TouchTap)
        << makeOp(ts + 2, EventType::Shortcut)
        << makeOp(ts + 3, EventType::DialogOpen)
        << makeOp(ts + 4, EventType::DialogClose)
        << makeOp(ts + 5, EventType::AreaClick);
    EXPECT_TRUE(Database::instance().batchInsert(ops));
    // 验证每种类型都能查回来
    for (const auto& op : ops) {
        QueryFilter f;
        f.startTime = QDateTime::fromMSecsSinceEpoch(op.timestamp - 1);
        f.endTime = QDateTime::fromMSecsSinceEpoch(op.timestamp + 1);
        f.eventType = QString::fromLatin1(eventTypeToString(op.eventType));
        f.limit = 10;
        auto result = Database::instance().queryOperations(f);
        EXPECT_EQ(result.size(), 1);
        EXPECT_EQ(result[0].eventType, op.eventType);
    }
}

// ========== 查询 ==========

TEST_F(TestDatabase, testQueryByTimeRange) {
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    QList<Operation> batch;
    for (int i = 0; i < 100; ++i) {
        batch.append(makeOp(base + i * 1000)); // 每秒一条，共100秒
    }
    EXPECT_TRUE(Database::instance().batchInsert(batch));

    // 查 [base+10s, base+30s) → time <= end 含端点，所以用 base+29999 排除 base+30s
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base + 10000);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 29999);
    f.limit = 1000;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 20);
    // 验证有序
    for (int i = 1; i < ops.size(); ++i) {
        EXPECT_TRUE(ops[i-1].timestamp <= ops[i].timestamp);
    }
}

TEST_F(TestDatabase, testQueryBySessionId) {
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    Operation a = makeOp(ts);     a.sessionId = "sessA";
    Operation b = makeOp(ts + 1); b.sessionId = "sessB";
    Operation c = makeOp(ts + 2); c.sessionId = "sessA";
    EXPECT_TRUE(Database::instance().batchInsert({a, b, c}));

    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(ts - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(ts + 10);
    f.sessionId = "sessA";
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 2);
    for (const auto& op : ops) {
        EXPECT_EQ(op.sessionId, QString("sessA"));
    }
}

TEST_F(TestDatabase, testQueryByEventType) {
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    EXPECT_TRUE(Database::instance().batchInsert({
        makeOp(ts, EventType::MouseClick),
        makeOp(ts + 1, EventType::Shortcut),
        makeOp(ts + 2, EventType::MouseClick),
    }));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(ts - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(ts + 10);
    f.eventType = "mouse_click";
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 2);
}

TEST_F(TestDatabase, testQueryOnlyMainWindow) {
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    EXPECT_TRUE(Database::instance().batchInsert({
        makeOp(ts, EventType::MouseClick, "MainWin", true),
        makeOp(ts + 1, EventType::MouseClick, "Dialog", false),
        makeOp(ts + 2, EventType::MouseClick, "MainWin", true),
    }));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(ts - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(ts + 10);
    f.onlyMainWindow = true;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 2);
    for (const auto& op : ops) {
        EXPECT_TRUE(op.isMainWindow);
    }
}

TEST_F(TestDatabase, testQueryLimit) {
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    QList<Operation> batch;
    for (int i = 0; i < 200; ++i) {
        batch.append(makeOp(base + i));
    }
    EXPECT_TRUE(Database::instance().batchInsert(batch));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 1000);
    f.limit = 50;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 50);
}

TEST_F(TestDatabase, testCountOperations) {
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    EXPECT_TRUE(Database::instance().batchInsert({
        makeOp(base), makeOp(base + 1), makeOp(base + 2)
    }));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 10);
    EXPECT_EQ(Database::instance().countOperations(f), qint64(3));
}

TEST_F(TestDatabase, testCountOperationsBySessionId) {
    // 多 session 场景: countOperations 只应返回指定 session 的操作数
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    QList<Operation> ops;
    for (int i = 0; i < 5; ++i) {
        Operation op = makeOp(base + i);
        op.sessionId = "sessA";
        ops.append(op);
    }
    for (int i = 0; i < 3; ++i) {
        Operation op = makeOp(base + 100 + i);
        op.sessionId = "sessB";
        ops.append(op);
    }
    EXPECT_TRUE(Database::instance().batchInsert(ops));

    QueryFilter fa;
    fa.sessionId = "sessA";
    EXPECT_EQ(Database::instance().countOperations(fa), qint64(5));

    QueryFilter fb;
    fb.sessionId = "sessB";
    EXPECT_EQ(Database::instance().countOperations(fb), qint64(3));

    // 空 filter = 全表 count = 8
    EXPECT_EQ(Database::instance().countOperations(QueryFilter{}), qint64(8));
}

TEST_F(TestDatabase, testCountEmptyRange) {
    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addDays(-1);
    f.endTime = QDateTime::currentDateTime();
    EXPECT_EQ(Database::instance().countOperations(f), qint64(0));
}

// ========== 会话 ==========

TEST_F(TestDatabase, testInsertSession) {
    Session s;
    s.id = "sess1";
    s.startTime = QDateTime::currentMSecsSinceEpoch();
    s.endTime = s.startTime + 60000;
    s.durationSeconds = 60;
    s.operationCount = 10;
    EXPECT_TRUE(Database::instance().insertSession(s));

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare("SELECT id,start_time,end_time,duration_seconds,operation_count FROM sessions WHERE id=?");
    q.addBindValue("sess1");
    EXPECT_TRUE(q.exec());
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toString(), QString("sess1"));
    EXPECT_EQ(q.value(3).toInt(), 60);
    EXPECT_EQ(q.value(4).toInt(), 10);
}

TEST_F(TestDatabase, testUpdateSession) {
    Session s;
    s.id = "sess2";
    s.startTime = QDateTime::currentMSecsSinceEpoch();
    s.endTime = s.startTime;
    s.durationSeconds = 0;
    s.operationCount = 0;
    EXPECT_TRUE(Database::instance().insertSession(s));

    s.endTime = s.startTime + 120000;
    s.durationSeconds = 120;
    s.operationCount = 50;
    EXPECT_TRUE(Database::instance().updateSession(s));

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare("SELECT duration_seconds,operation_count FROM sessions WHERE id=?");
    q.addBindValue("sess2");
    EXPECT_TRUE(q.exec());
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toInt(), 120);
    EXPECT_EQ(q.value(1).toInt(), 50);
}

TEST_F(TestDatabase, testSessionLifecycleWithOperationCount) {
    // 模拟完整生命周期: insert session → write ops → count → update session
    // 先插入一个旧 session 的操作，验证 countOperations 不会把旧数据算进来
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < 10; ++i) {
        Operation old = makeOp(base - 100000 + i);
        old.sessionId = "old-session";
        Database::instance().insertOperation(old);
    }

    QString sid = "lifecycle-sess";

    // 1. 插入 session (start)
    Session s;
    s.id = sid;
    s.startTime = base;
    s.endTime = 0;
    s.durationSeconds = 0;
    s.operationCount = 0;
    EXPECT_TRUE(Database::instance().insertSession(s));

    // 2. 写入 5 条操作
    for (int i = 0; i < 5; ++i) {
        Operation op = makeOp(s.startTime + i * 1000);
        op.sessionId = sid;
        EXPECT_TRUE(Database::instance().insertOperation(op));
    }

    // 3. 统计操作数 — 必须按 session_id 过滤，否则会把旧 session 的 10 条也算进来
    QueryFilter countFilter;
    countFilter.sessionId = sid;
    int count = static_cast<int>(Database::instance().countOperations(countFilter));
    EXPECT_EQ(count, 5);

    // 4. 更新 session (end)
    s.endTime = s.startTime + 10000;
    s.durationSeconds = 10;
    s.operationCount = count;
    EXPECT_TRUE(Database::instance().updateSession(s));

    // 5. 验证 session 数据正确
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare("SELECT end_time,duration_seconds,operation_count FROM sessions WHERE id=?");
    q.addBindValue(sid);
    EXPECT_TRUE(q.exec());
    EXPECT_TRUE(q.next());
    EXPECT_GT(q.value(0).toLongLong(), 0);   // end_time 已设置
    EXPECT_EQ(q.value(1).toInt(), 10);        // duration_seconds
    EXPECT_EQ(q.value(2).toInt(), 5);         // operation_count = 5 (不是 15)
}

TEST_F(TestDatabase, testSessionNotLostAfterReopen) {
    // 验证 session 在数据库重开后仍然存在
    Session s;
    s.id = "reopen-sess";
    s.startTime = QDateTime::currentMSecsSinceEpoch();
    s.endTime = s.startTime + 5000;
    s.durationSeconds = 5;
    s.operationCount = 3;
    EXPECT_TRUE(Database::instance().insertSession(s));

    // 关闭并重新打开
    Database::instance().close();
    EXPECT_TRUE(Database::instance().open(path_));

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare("SELECT id,operation_count FROM sessions WHERE id=?");
    q.addBindValue("reopen-sess");
    EXPECT_TRUE(q.exec());
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toString(), QString("reopen-sess"));
    EXPECT_EQ(q.value(1).toInt(), 3);
}

TEST_F(TestDatabase, testRecoverUnclosedSessions) {
    // 模拟程序崩溃: session 插入但 end_time=0, 有操作记录
    QString sid = "crash-sess";
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    Session s;
    s.id = sid;
    s.startTime = startTime;
    s.endTime = 0;  // 未关闭
    s.durationSeconds = 0;
    s.operationCount = 0;
    EXPECT_TRUE(Database::instance().insertSession(s));

    // 写入 3 条操作
    for (int i = 0; i < 3; ++i) {
        Operation op = makeOp(startTime + i * 1000);
        op.sessionId = sid;
        Database::instance().insertOperation(op);
    }

    // 恢复未关闭的会话
    int recovered = Database::instance().recoverUnclosedSessions();
    EXPECT_EQ(recovered, 1);

    // 验证 session 已被正确关闭
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare("SELECT end_time,duration_seconds,operation_count FROM sessions WHERE id=?");
    q.addBindValue(sid);
    EXPECT_TRUE(q.exec());
    EXPECT_TRUE(q.next());
    EXPECT_GT(q.value(0).toLongLong(), 0);   // end_time 已设置
    EXPECT_GT(q.value(1).toInt(), 0);         // duration_seconds > 0
    EXPECT_EQ(q.value(2).toInt(), 3);         // operation_count = 3
}

TEST_F(TestDatabase, testRecoverUnclosedSessionsNone) {
    // 所有 session 都已正常关闭, 不应恢复任何会话
    Session s1;
    s1.id = "closed-sess-1";
    s1.startTime = QDateTime::currentMSecsSinceEpoch();
    s1.endTime = s1.startTime + 10000;
    s1.durationSeconds = 10;
    s1.operationCount = 5;
    Database::instance().insertSession(s1);

    int recovered = Database::instance().recoverUnclosedSessions();
    EXPECT_EQ(recovered, 0);
}

TEST_F(TestDatabase, testRecoverMultipleUnclosedSessions) {
    // 多个未关闭的 session 都应被恢复
    for (int i = 0; i < 3; ++i) {
        Session s;
        s.id = QString("crash-sess-%1").arg(i);
        s.startTime = QDateTime::currentMSecsSinceEpoch() + i * 1000;
        s.endTime = 0;
        Database::instance().insertSession(s);

        Operation op = makeOp(s.startTime + 500);
        op.sessionId = s.id;
        Database::instance().insertOperation(op);
    }

    int recovered = Database::instance().recoverUnclosedSessions();
    EXPECT_EQ(recovered, 3);

    // 验证全部已关闭
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.exec("SELECT COUNT(*) FROM sessions WHERE (end_time IS NULL OR end_time = 0)");
    EXPECT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toInt(), 0);  // 没有未关闭的了
}

// ========== 清理 ==========

TEST_F(TestDatabase, testCleanOldData) {
    // 插入一条 10 天前的数据
    qint64 oldTs = QDateTime::currentDateTime().addDays(-10).toMSecsSinceEpoch();
    EXPECT_TRUE(Database::instance().insertOperation(makeOp(oldTs)));
    // 插入一条当前数据
    qint64 nowTs = QDateTime::currentMSecsSinceEpoch();
    EXPECT_TRUE(Database::instance().insertOperation(makeOp(nowTs)));

    int removed = Database::instance().cleanOldData(7); // 保留7天
    EXPECT_TRUE(removed >= 1);

    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addDays(-30);
    f.endTime = QDateTime::currentDateTime().addDays(1);
    f.limit = 100;
    auto ops = Database::instance().queryOperations(f);
    // 只剩当前数据
    EXPECT_EQ(ops.size(), 1);
    EXPECT_TRUE(ops[0].timestamp >= nowTs - 1);
}

// ========== 导出 ==========

TEST_F(TestDatabase, testExportRawData) {
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    EXPECT_TRUE(Database::instance().batchInsert({
        makeOp(base), makeOp(base + 1), makeOp(base + 2)
    }));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 10);
    auto exported = Database::instance().exportRawData(f);
    EXPECT_EQ(exported.size(), 3);
}

// ========== 数据完整性 ==========

TEST_F(TestDatabase, testOperationRoundTrip) {
    Operation op;
    op.sessionId = "round-trip";
    op.timestamp = QDateTime::currentMSecsSinceEpoch();
    op.eventType = EventType::Shortcut;
    op.inputMethod = InputMethod::Keyboard;
    op.controlClass = "QLineEdit";
    op.controlName = "editUser";
    op.controlText = "hello";
    op.controlPath = "MainWindow > CentralWidget > editUser";
    op.actionName = "copy";
    op.keySequence = "Ctrl+C";
    op.windowClass = "MainWindow";
    op.windowTitle = "App - Main";
    op.windowPath = "MainWindow";
    op.isMainWindow = true;
    op.screenX = 300;
    op.screenY = 400;
    op.heatRegion = 77;
    op.duration = 5000;

    EXPECT_TRUE(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(op.timestamp - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(op.timestamp + 1);
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 1);
    const auto& r = ops[0];
    EXPECT_EQ(r.sessionId, op.sessionId);
    EXPECT_EQ(r.timestamp, op.timestamp);
    EXPECT_EQ(r.eventType, op.eventType);
    EXPECT_EQ(r.inputMethod, op.inputMethod);
    EXPECT_EQ(r.controlClass, op.controlClass);
    EXPECT_EQ(r.controlName, op.controlName);
    EXPECT_EQ(r.controlText, op.controlText);
    EXPECT_EQ(r.controlPath, op.controlPath);
    EXPECT_EQ(r.actionName, op.actionName);
    EXPECT_EQ(r.keySequence, op.keySequence);
    EXPECT_EQ(r.windowClass, op.windowClass);
    EXPECT_EQ(r.windowTitle, op.windowTitle);
    EXPECT_EQ(r.windowPath, op.windowPath);
    EXPECT_EQ(r.isMainWindow, op.isMainWindow);
    EXPECT_EQ(r.screenX, op.screenX);
    EXPECT_EQ(r.screenY, op.screenY);
    EXPECT_EQ(r.heatRegion, op.heatRegion);
    EXPECT_TRUE(r.duration.has_value());
    EXPECT_EQ(*r.duration, 5000);
}

TEST_F(TestDatabase, testDurationOptional) {
    // 不设 duration（nullopt）
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    Operation op = makeOp(base, EventType::MouseClick);
    // duration 默认 nullopt
    EXPECT_TRUE(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 1);
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 1);
    EXPECT_TRUE(!ops[0].duration.has_value());

    // 设 duration — 用不同时间戳避免查到第一条
    qint64 ts2 = base + 10000;
    Operation op2 = makeOp(ts2, EventType::MouseClick);
    op2.duration = 3000;
    EXPECT_TRUE(Database::instance().insertOperation(op2));
    QueryFilter f2;
    f2.startTime = QDateTime::fromMSecsSinceEpoch(ts2 - 1);
    f2.endTime = QDateTime::fromMSecsSinceEpoch(ts2 + 1);
    ops = Database::instance().queryOperations(f2);
    EXPECT_EQ(ops.size(), 1);
    EXPECT_TRUE(ops[0].duration.has_value());
    EXPECT_EQ(*ops[0].duration, 3000);
}

TEST_F(TestDatabase, testHeatRegionNegative) {
    Operation op = makeOp(QDateTime::currentMSecsSinceEpoch());
    op.heatRegion = -1; // 无效区域
    EXPECT_TRUE(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(op.timestamp - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(op.timestamp + 1);
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].heatRegion, -1);
}

// ========== 线程安全 ==========

TEST_F(TestDatabase, testConnectionPerThread) {
    // 主线程插入数据
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    EXPECT_TRUE(Database::instance().insertOperation(makeOp(base)));

    // 子线程查询（用 std::thread，不需要事件循环）
    int resultCount = -1;
    std::thread t([&]() {
        QueryFilter f;
        f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
        f.endTime = QDateTime::fromMSecsSinceEpoch(base + 10);
        f.limit = 100;
        resultCount = Database::instance().queryOperations(f).size();
    });
    t.join();
    EXPECT_EQ(resultCount, 1);
}

