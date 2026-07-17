// TestBatchWriter.cpp — 批量写入器：写入/flush/信号/线程
#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QThread>
#include <QElapsedTimer>

#include "storage/database.h"
#include "storage/operation_queue.h"
#include "storage/batch_writer.h"
#include "core/types.h"
#include "core/config.h"

using namespace ui_shared::behavior;

namespace {
Operation makeOp(int id) {
    Operation op;
    op.sessionId = "bw-test";
    op.timestamp = QDateTime::currentMSecsSinceEpoch() + id;
    op.eventType = EventType::MouseClick;
    op.inputMethod = InputMethod::Mouse;
    op.windowClass = "MainWindow";
    op.isMainWindow = true;
    return op;
}
} // namespace

class TestBatchWriter : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;


protected:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
};

void TestBatchWriter::SetUp() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/bw_test.db";
    Config::instance().setDatabasePath(path_);
    Config::instance().setBatchSize(10);
    Config::instance().setBatchTimeoutMs(100);
    Database::instance().open(path_);
}

void TestBatchWriter::TearDown() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

TEST_F(TestBatchWriter, testStartStop) {
    OperationQueue queue;
    BatchWriter writer(&queue);
    writer.start();
    // 入队一条数据，flush 验证线程在运行
    queue.enqueue(makeOp(0));
    QSignalSpy spy(&writer, &BatchWriter::written);
    writer.flush();
    EXPECT_TRUE(spy.wait(2000)); // 线程在运行才能收到信号
    writer.stop();
    // stop 后再 flush 不应触发（线程已退出）
    QSignalSpy spy2(&writer, &BatchWriter::written);
    writer.flush();
    EXPECT_TRUE(!spy2.wait(500));
}

TEST_F(TestBatchWriter, testFlushWritesAll) {
    OperationQueue queue;
    for (int i = 0; i < 25; ++i) {
        queue.enqueue(makeOp(i));
    }

    BatchWriter writer(&queue);
    writer.start();
    writer.flush();
    // 等待写入完成
    QThread::msleep(200);
    writer.stop();

    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addSecs(-10);
    f.endTime = QDateTime::currentDateTime().addSecs(10);
    f.limit = 1000;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 25);
}

TEST_F(TestBatchWriter, testWrittenSignal) {
    OperationQueue queue;
    for (int i = 0; i < 5; ++i) {
        queue.enqueue(makeOp(i));
    }

    BatchWriter writer(&queue);
    QSignalSpy spy(&writer, &BatchWriter::written);
    writer.start();
    writer.flush();
    spy.wait(1000);
    writer.stop();

    EXPECT_TRUE(spy.count() >= 1);
    // 总写入数 = 5
    int totalWritten = 0;
    for (int i = 0; i < spy.count(); ++i) {
        totalWritten += spy.at(i).at(0).toInt();
    }
    EXPECT_EQ(totalWritten, 5);
}

TEST_F(TestBatchWriter, testEmptyQueueNoWrite) {
    OperationQueue queue;
    BatchWriter writer(&queue);
    QSignalSpy spy(&writer, &BatchWriter::written);
    writer.start();
    QThread::msleep(300); // 等几个 tick
    writer.stop();
    // 空队列不应触发 written
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(TestBatchWriter, testBatchSizeRespected) {
    // batchSize=10, 入队 25 条
    // 定时器触发时应每次最多写 10 条
    OperationQueue queue;
    for (int i = 0; i < 25; ++i) {
        queue.enqueue(makeOp(i));
    }

    BatchWriter writer(&queue);
    QSignalSpy spy(&writer, &BatchWriter::written);
    writer.start();
    // 等第一个 tick (100ms)
    spy.wait(500);
    writer.stop();

    // 第一个批次不应超过 batchSize
    if (spy.count() > 0) {
        int firstBatch = spy.at(0).at(0).toInt();
        EXPECT_TRUE(firstBatch <= 10);
    }
}

TEST_F(TestBatchWriter, testStopFlushesRemaining) {
    OperationQueue queue;
    for (int i = 0; i < 15; ++i) {
        queue.enqueue(makeOp(i));
    }

    BatchWriter writer(&queue);
    writer.start();
    QThread::msleep(50); // 让定时器还没来得及写
    writer.stop();       // stop 应 flush 剩余

    QueryFilter f;
    f.startTime = QDateTime::currentDateTime().addSecs(-10);
    f.endTime = QDateTime::currentDateTime().addSecs(10);
    f.limit = 1000;
    auto ops = Database::instance().queryOperations(f);
    EXPECT_EQ(ops.size(), 15);
}

