// TestDatabase.cpp — 数据库 CRUD + Schema + 线程连接
#include <QtTest/QtTest>
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

class TestDatabase : public QObject {
    Q_OBJECT
private slots:
    // ===== 生命周期 =====
    void init();
    void cleanup();

    // ===== Schema =====
    void testOpenCreatesAllTables();
    void testOpenCreatesIndexes();
    void testReopenSamePath();

    // ===== 插入 =====
    void testInsertSingleOperation();
    void testBatchInsert();
    void testBatchInsertEmpty();
    void testInsertAllEventTypes();

    // ===== 查询 =====
    void testQueryByTimeRange();
    void testQueryBySessionId();
    void testQueryByEventType();
    void testQueryOnlyMainWindow();
    void testQueryLimit();
    void testCountOperations();
    void testCountEmptyRange();

    // ===== 会话 =====
    void testInsertSession();
    void testUpdateSession();

    // ===== 清理 =====
    void testCleanOldData();

    // ===== 导出 =====
    void testExportRawData();

    // ===== 数据完整性 =====
    void testOperationRoundTrip();
    void testDurationOptional();
    void testHeatRegionNegative();

    // ===== 线程安全 =====
    void testConnectionPerThread();

private:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
};

void TestDatabase::init() {
    dir_ = new QTemporaryDir;
    path_ = tempDbPath(*dir_);
    Config::instance().setDatabasePath(path_);
    QVERIFY(Database::instance().open(path_));
}

void TestDatabase::cleanup() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

// ========== Schema ==========

void TestDatabase::testOpenCreatesAllTables() {
    QSqlDatabase db = Database::instance().connection();
    QStringList expected = {
        "metadata", "sessions", "operations",
        "agg_operation_stats", "agg_module_stats", "agg_input_stats",
        "agg_heatmap_stats", "agg_dialog_stats", "agg_time_distribution"
    };
    QSqlQuery q(db);
    QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"));
    QStringList actual;
    while (q.next()) actual << q.value(0).toString();
    for (const auto& t : expected) {
        QVERIFY2(actual.contains(t), qPrintable("Missing table: " + t));
    }
}

void TestDatabase::testOpenCreatesIndexes() {
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE type='index' ORDER BY name"));
    QStringList indexes;
    while (q.next()) indexes << q.value(0).toString();
    QVERIFY(indexes.contains("idx_ops_time"));
    QVERIFY(indexes.contains("idx_ops_session"));
    QVERIFY(indexes.contains("idx_ops_event"));
    QVERIFY(indexes.contains("idx_agg_ops_time"));
}

void TestDatabase::testReopenSamePath() {
    Database::instance().close();
    QVERIFY(Database::instance().open(path_));
    QVERIFY(Database::instance().isOpen());
    // 数据应该还在
    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    QVERIFY(q.exec("SELECT COUNT(*) FROM operations"));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toLongLong(), qint64(0)); // 没插过数据
}

// ========== 插入 ==========

void TestDatabase::testInsertSingleOperation() {
    Operation op = makeOp(QDateTime::currentMSecsSinceEpoch());
    QVERIFY(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addSecs(-60);
    f.endTime = QDateTime::currentDateTime().addSecs(60);
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 1);
    QCOMPARE(ops[0].controlClass, QString("QPushButton"));
}

void TestDatabase::testBatchInsert() {
    QList<Operation> batch;
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < 50; ++i)
        batch.append(makeOp(base + i));
    QVERIFY(Database::instance().batchInsert(batch));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 100);
    f.limit = 100;
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 50);
}

void TestDatabase::testBatchInsertEmpty() {
    QVERIFY(Database::instance().batchInsert({}));
    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addSecs(-60);
    f.endTime = QDateTime::currentDateTime().addSecs(60);
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 0);
}

void TestDatabase::testInsertAllEventTypes() {
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    QList<Operation> ops;
    ops << makeOp(ts, EventType::MouseClick)
        << makeOp(ts + 1, EventType::TouchTap)
        << makeOp(ts + 2, EventType::Shortcut)
        << makeOp(ts + 3, EventType::DialogOpen)
        << makeOp(ts + 4, EventType::DialogClose)
        << makeOp(ts + 5, EventType::AreaClick);
    QVERIFY(Database::instance().batchInsert(ops));
    // 验证每种类型都能查回来
    for (const auto& op : ops) {
        QueryFilter f;
        f.startTime = QDateTime::fromMSecsSinceEpoch(op.timestamp - 1);
        f.endTime = QDateTime::fromMSecsSinceEpoch(op.timestamp + 1);
        f.eventType = QString::fromLatin1(eventTypeToString(op.eventType));
        f.limit = 10;
        auto result = Database::instance().queryOperations(f);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].eventType, op.eventType);
    }
}

// ========== 查询 ==========

void TestDatabase::testQueryByTimeRange() {
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    QList<Operation> batch;
    for (int i = 0; i < 100; ++i)
        batch.append(makeOp(base + i * 1000)); // 每秒一条，共100秒
    QVERIFY(Database::instance().batchInsert(batch));

    // 查 [base+10s, base+30s) → time <= end 含端点，所以用 base+29999 排除 base+30s
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base + 10000);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 29999);
    f.limit = 1000;
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 20);
    // 验证有序
    for (int i = 1; i < ops.size(); ++i)
        QVERIFY(ops[i-1].timestamp <= ops[i].timestamp);
}

void TestDatabase::testQueryBySessionId() {
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    Operation a = makeOp(ts);     a.sessionId = "sessA";
    Operation b = makeOp(ts + 1); b.sessionId = "sessB";
    Operation c = makeOp(ts + 2); c.sessionId = "sessA";
    QVERIFY(Database::instance().batchInsert({a, b, c}));

    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(ts - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(ts + 10);
    f.sessionId = "sessA";
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 2);
    for (const auto& op : ops)
        QCOMPARE(op.sessionId, QString("sessA"));
}

void TestDatabase::testQueryByEventType() {
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(Database::instance().batchInsert({
        makeOp(ts, EventType::MouseClick),
        makeOp(ts + 1, EventType::Shortcut),
        makeOp(ts + 2, EventType::MouseClick),
    }));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(ts - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(ts + 10);
    f.eventType = "mouse_click";
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 2);
}

void TestDatabase::testQueryOnlyMainWindow() {
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(Database::instance().batchInsert({
        makeOp(ts, EventType::MouseClick, "MainWin", true),
        makeOp(ts + 1, EventType::MouseClick, "Dialog", false),
        makeOp(ts + 2, EventType::MouseClick, "MainWin", true),
    }));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(ts - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(ts + 10);
    f.onlyMainWindow = true;
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 2);
    for (const auto& op : ops)
        QVERIFY(op.isMainWindow);
}

void TestDatabase::testQueryLimit() {
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    QList<Operation> batch;
    for (int i = 0; i < 200; ++i)
        batch.append(makeOp(base + i));
    QVERIFY(Database::instance().batchInsert(batch));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 1000);
    f.limit = 50;
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 50);
}

void TestDatabase::testCountOperations() {
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(Database::instance().batchInsert({
        makeOp(base), makeOp(base + 1), makeOp(base + 2)
    }));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 10);
    QCOMPARE(Database::instance().countOperations(f), qint64(3));
}

void TestDatabase::testCountEmptyRange() {
    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addDays(-1);
    f.endTime = QDateTime::currentDateTime();
    QCOMPARE(Database::instance().countOperations(f), qint64(0));
}

// ========== 会话 ==========

void TestDatabase::testInsertSession() {
    Session s;
    s.id = "sess1";
    s.startTime = QDateTime::currentMSecsSinceEpoch();
    s.endTime = s.startTime + 60000;
    s.durationSeconds = 60;
    s.operationCount = 10;
    QVERIFY(Database::instance().insertSession(s));

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare("SELECT id,start_time,end_time,duration_seconds,operation_count FROM sessions WHERE id=?");
    q.addBindValue("sess1");
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QString("sess1"));
    QCOMPARE(q.value(3).toInt(), 60);
    QCOMPARE(q.value(4).toInt(), 10);
}

void TestDatabase::testUpdateSession() {
    Session s;
    s.id = "sess2";
    s.startTime = QDateTime::currentMSecsSinceEpoch();
    s.endTime = s.startTime;
    s.durationSeconds = 0;
    s.operationCount = 0;
    QVERIFY(Database::instance().insertSession(s));

    s.endTime = s.startTime + 120000;
    s.durationSeconds = 120;
    s.operationCount = 50;
    QVERIFY(Database::instance().updateSession(s));

    QSqlDatabase db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare("SELECT duration_seconds,operation_count FROM sessions WHERE id=?");
    q.addBindValue("sess2");
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 120);
    QCOMPARE(q.value(1).toInt(), 50);
}

// ========== 清理 ==========

void TestDatabase::testCleanOldData() {
    // 插入一条 10 天前的数据
    qint64 oldTs = QDateTime::currentDateTime().addDays(-10).toMSecsSinceEpoch();
    QVERIFY(Database::instance().insertOperation(makeOp(oldTs)));
    // 插入一条当前数据
    qint64 nowTs = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(Database::instance().insertOperation(makeOp(nowTs)));

    int removed = Database::instance().cleanOldData(7); // 保留7天
    QVERIFY(removed >= 1);

    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addDays(-30);
    f.endTime = QDateTime::currentDateTime().addDays(1);
    f.limit = 100;
    auto ops = Database::instance().queryOperations(f);
    // 只剩当前数据
    QCOMPARE(ops.size(), 1);
    QVERIFY(ops[0].timestamp >= nowTs - 1);
}

// ========== 导出 ==========

void TestDatabase::testExportRawData() {
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(Database::instance().batchInsert({
        makeOp(base), makeOp(base + 1), makeOp(base + 2)
    }));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 10);
    auto exported = Database::instance().exportRawData(f);
    QCOMPARE(exported.size(), 3);
}

// ========== 数据完整性 ==========

void TestDatabase::testOperationRoundTrip() {
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

    QVERIFY(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(op.timestamp - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(op.timestamp + 1);
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 1);
    const auto& r = ops[0];
    QCOMPARE(r.sessionId, op.sessionId);
    QCOMPARE(r.timestamp, op.timestamp);
    QCOMPARE(r.eventType, op.eventType);
    QCOMPARE(r.inputMethod, op.inputMethod);
    QCOMPARE(r.controlClass, op.controlClass);
    QCOMPARE(r.controlName, op.controlName);
    QCOMPARE(r.controlText, op.controlText);
    QCOMPARE(r.controlPath, op.controlPath);
    QCOMPARE(r.actionName, op.actionName);
    QCOMPARE(r.keySequence, op.keySequence);
    QCOMPARE(r.windowClass, op.windowClass);
    QCOMPARE(r.windowTitle, op.windowTitle);
    QCOMPARE(r.windowPath, op.windowPath);
    QCOMPARE(r.isMainWindow, op.isMainWindow);
    QCOMPARE(r.screenX, op.screenX);
    QCOMPARE(r.screenY, op.screenY);
    QCOMPARE(r.heatRegion, op.heatRegion);
    QVERIFY(r.duration.has_value());
    QCOMPARE(*r.duration, 5000);
}

void TestDatabase::testDurationOptional() {
    // 不设 duration（nullopt）
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    Operation op = makeOp(base, EventType::MouseClick);
    // duration 默认 nullopt
    QVERIFY(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(base - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(base + 1);
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 1);
    QVERIFY(!ops[0].duration.has_value());

    // 设 duration — 用不同时间戳避免查到第一条
    qint64 ts2 = base + 10000;
    Operation op2 = makeOp(ts2, EventType::MouseClick);
    op2.duration = 3000;
    QVERIFY(Database::instance().insertOperation(op2));
    QueryFilter f2;
    f2.startTime = QDateTime::fromMSecsSinceEpoch(ts2 - 1);
    f2.endTime = QDateTime::fromMSecsSinceEpoch(ts2 + 1);
    ops = Database::instance().queryOperations(f2);
    QCOMPARE(ops.size(), 1);
    QVERIFY(ops[0].duration.has_value());
    QCOMPARE(*ops[0].duration, 3000);
}

void TestDatabase::testHeatRegionNegative() {
    Operation op = makeOp(QDateTime::currentMSecsSinceEpoch());
    op.heatRegion = -1; // 无效区域
    QVERIFY(Database::instance().insertOperation(op));
    QueryFilter f;
    f.startTime = QDateTime::fromMSecsSinceEpoch(op.timestamp - 1);
    f.endTime = QDateTime::fromMSecsSinceEpoch(op.timestamp + 1);
    auto ops = Database::instance().queryOperations(f);
    QCOMPARE(ops.size(), 1);
    QCOMPARE(ops[0].heatRegion, -1);
}

// ========== 线程安全 ==========

void TestDatabase::testConnectionPerThread() {
    // 主线程插入数据
    qint64 base = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(Database::instance().insertOperation(makeOp(base)));

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
    QCOMPARE(resultCount, 1);
}

QTEST_MAIN(TestDatabase)
#include "TestDatabase.moc"
