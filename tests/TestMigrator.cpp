// TestMigrator.cpp — 数据库迁移版本管理
#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "storage/Database.h"
#include "storage/Migrator.h"
#include "core/Config.h"

using namespace ui_shared::behavior;

class TestMigrator : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void testFreshDbGetsCurrentVersion();
    void testGetVersionAfterOpen();
    void testSetVersion();
    void testMigrateIdempotent();
    void testMigrateFromV1();

private:
    QTemporaryDir* m_dir = nullptr;
    QString m_path;
};

void TestMigrator::init() {
    m_dir = new QTemporaryDir;
    m_path = m_dir->path() + "/mig_test.db";
    Config::instance().setDatabasePath(m_path);
    Database::instance().open(m_path);
}

void TestMigrator::cleanup() {
    Database::instance().close();
    delete m_dir;
    m_dir = nullptr;
}

void TestMigrator::testFreshDbGetsCurrentVersion() {
    QSqlDatabase db = Database::instance().connection();
    // open() 内部已调用 Migrator::migrate()
    QCOMPARE(Migrator::getVersion(db), Migrator::CURRENT_VERSION);
}

void TestMigrator::testGetVersionAfterOpen() {
    QSqlDatabase db = Database::instance().connection();
    QVERIFY(Migrator::getVersion(db) > 0);
}

void TestMigrator::testSetVersion() {
    QSqlDatabase db = Database::instance().connection();
    QVERIFY(Migrator::setVersion(db, 99));
    QCOMPARE(Migrator::getVersion(db), 99);
    // 恢复
    Migrator::setVersion(db, Migrator::CURRENT_VERSION);
}

void TestMigrator::testMigrateIdempotent() {
    QSqlDatabase db = Database::instance().connection();
    int v1 = Migrator::getVersion(db);
    QVERIFY(Migrator::migrate(db));
    int v2 = Migrator::getVersion(db);
    QCOMPARE(v1, v2); // 已是最新版本，再 migrate 不变
}

void TestMigrator::testMigrateFromV1() {
    QSqlDatabase db = Database::instance().connection();
    // 模拟 v1 数据库
    Migrator::setVersion(db, 1);
    QCOMPARE(Migrator::getVersion(db), 1);

    // 执行迁移
    QVERIFY(Migrator::migrate(db));
    QCOMPARE(Migrator::getVersion(db), Migrator::CURRENT_VERSION);
}

QTEST_MAIN(TestMigrator)
#include "TestMigrator.moc"
