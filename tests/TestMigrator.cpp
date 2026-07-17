// TestMigrator.cpp — 数据库迁移版本管理
#include <gtest/gtest.h>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "storage/database.h"
#include "storage/migrator.h"
#include "core/config.h"

using namespace ui_shared::behavior;

class TestMigrator : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;


protected:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
};

void TestMigrator::SetUp() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/mig_test.db";
    Config::instance().setDatabasePath(path_);
    Database::instance().open(path_);
}

void TestMigrator::TearDown() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

TEST_F(TestMigrator, testFreshDbGetsCurrentVersion) {
    QSqlDatabase db = Database::instance().connection();
    // open() 内部已调用 Migrator::migrate()
    EXPECT_EQ(Migrator::getVersion(db), Migrator::CURRENT_VERSION);
}

TEST_F(TestMigrator, testGetVersionAfterOpen) {
    QSqlDatabase db = Database::instance().connection();
    EXPECT_TRUE(Migrator::getVersion(db) > 0);
}

TEST_F(TestMigrator, testSetVersion) {
    QSqlDatabase db = Database::instance().connection();
    EXPECT_TRUE(Migrator::setVersion(db, 99));
    EXPECT_EQ(Migrator::getVersion(db), 99);
    // 恢复
    Migrator::setVersion(db, Migrator::CURRENT_VERSION);
}

TEST_F(TestMigrator, testMigrateIdempotent) {
    QSqlDatabase db = Database::instance().connection();
    int v1 = Migrator::getVersion(db);
    EXPECT_TRUE(Migrator::migrate(db));
    int v2 = Migrator::getVersion(db);
    EXPECT_EQ(v1, v2); // 已是最新版本，再 migrate 不变
}

TEST_F(TestMigrator, testMigrateFromV1) {
    QSqlDatabase db = Database::instance().connection();
    // 模拟 v1 数据库
    Migrator::setVersion(db, 1);
    EXPECT_EQ(Migrator::getVersion(db), 1);

    // 执行迁移
    EXPECT_TRUE(Migrator::migrate(db));
    EXPECT_EQ(Migrator::getVersion(db), Migrator::CURRENT_VERSION);
}

