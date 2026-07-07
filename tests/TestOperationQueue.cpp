// TestOperationQueue.cpp — 队列入队/出队/背压/线程安全
#include <QtTest/QtTest>
#include <QThread>
#include <QList>
#include <atomic>
#include <thread>
#include <vector>

#include "storage/operation_queue.h"
#include "core/types.h"

using namespace ui_shared::behavior;

namespace {
Operation makeOp(int id) {
    Operation op;
    op.sessionId = QString("sess-%1").arg(id);
    op.timestamp = id;
    return op;
}
} // namespace

class TestOperationQueue : public QObject {
    Q_OBJECT
private slots:
    void testEnqueueDequeue();
    void testDequeueEmpty();
    void testDequeuePartial();
    void testSize();
    void testFIFO();
    void testMoveSemantics();
    void testHighWatermarkDrop();
    void testDroppedCount();
    void testWaitForDataTimeout();
    void testWaitForDataAvailable();
    void testConcurrentProducerConsumer();
    void testDequeueZero();
    void testDequeueNegative();
};

void TestOperationQueue::testEnqueueDequeue() {
    OperationQueue q;
    q.enqueue(makeOp(1));
    q.enqueue(makeOp(2));
    auto ops = q.dequeue(10);
    QCOMPARE(ops.size(), 2);
    QCOMPARE(ops[0].sessionId, QString("sess-1"));
    QCOMPARE(ops[1].sessionId, QString("sess-2"));
}

void TestOperationQueue::testDequeueEmpty() {
    OperationQueue q;
    auto ops = q.dequeue(10);
    QVERIFY(ops.isEmpty());
}

void TestOperationQueue::testDequeuePartial() {
    OperationQueue q;
    for (int i = 0; i < 10; ++i)
        q.enqueue(makeOp(i));
    auto ops = q.dequeue(3);
    QCOMPARE(ops.size(), 3);
    QCOMPARE(q.size(), 7);
}

void TestOperationQueue::testSize() {
    OperationQueue q;
    QCOMPARE(q.size(), 0);
    q.enqueue(makeOp(1));
    q.enqueue(makeOp(2));
    QCOMPARE(q.size(), 2);
    q.dequeue(1);
    QCOMPARE(q.size(), 1);
}

void TestOperationQueue::testFIFO() {
    OperationQueue q;
    for (int i = 0; i < 100; ++i)
        q.enqueue(makeOp(i));
    auto ops = q.dequeue(100);
    QCOMPARE(ops.size(), 100);
    for (int i = 0; i < 100; ++i)
        QCOMPARE(ops[i].timestamp, qint64(i));
}

void TestOperationQueue::testMoveSemantics() {
    OperationQueue q;
    Operation op;
    op.sessionId = "moved";
    op.timestamp = 999;
    q.enqueue(std::move(op));
    auto ops = q.dequeue(1);
    QCOMPARE(ops[0].sessionId, QString("moved"));
    QCOMPARE(ops[0].timestamp, qint64(999));
}

void TestOperationQueue::testHighWatermarkDrop() {
    OperationQueue q;
    // HIGH_WATERMARK = 50000
    for (int i = 0; i < 50000; ++i)
        q.enqueue(makeOp(i));
    QCOMPARE(q.size(), 50000);
    // 再入队应该被丢弃
    q.enqueue(makeOp(50000));
    QCOMPARE(q.size(), 50000); // 不变
    QVERIFY(q.droppedCount() > 0);
}

void TestOperationQueue::testDroppedCount() {
    OperationQueue q;
    QCOMPARE(q.droppedCount(), qint64(0));
    for (int i = 0; i < 50000; ++i)
        q.enqueue(makeOp(i));
    for (int i = 0; i < 100; ++i)
        q.enqueue(makeOp(50000 + i)); // 全部丢弃
    QCOMPARE(q.droppedCount(), qint64(100));
}

void TestOperationQueue::testWaitForDataTimeout() {
    OperationQueue q;
    // 空队列，应该超时返回
    QElapsedTimer timer;
    timer.start();
    bool result = q.waitForData(50);
    QVERIFY(!result);
    QVERIFY(timer.elapsed() >= 40);  // 至少等了 ~50ms
}

void TestOperationQueue::testWaitForDataAvailable() {
    OperationQueue q;
    q.enqueue(makeOp(1));
    bool result = q.waitForData(100);
    QVERIFY(result);
}

void TestOperationQueue::testConcurrentProducerConsumer() {
    OperationQueue q;
    const int PRODUCER_COUNT = 4;
    const int ITEMS_PER_PRODUCER = 1000;
    std::atomic<int> totalDequeued{0};

    // 消费者线程（std::thread，不需要事件循环）
    std::thread consumer([&]() {
        while (totalDequeued.load() < PRODUCER_COUNT * ITEMS_PER_PRODUCER) {
            if (q.waitForData(100)) {
                auto ops = q.dequeue(100);
                totalDequeued.fetch_add(ops.size());
            }
        }
    });

    // 生产者线程
    std::vector<std::thread> producers;
    for (int p = 0; p < PRODUCER_COUNT; ++p) {
        producers.emplace_back([&q, p]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i)
                q.enqueue(makeOp(p * ITEMS_PER_PRODUCER + i));
        });
    }

    // 等待生产者完成
    for (auto& t : producers) t.join();

    // 等待消费者完成
    consumer.join();

    QCOMPARE(totalDequeued.load(), PRODUCER_COUNT * ITEMS_PER_PRODUCER);
    QCOMPARE(q.size(), 0);
}

void TestOperationQueue::testDequeueZero() {
    OperationQueue q;
    q.enqueue(makeOp(1));
    auto ops = q.dequeue(0);
    QVERIFY(ops.isEmpty());
    QCOMPARE(q.size(), 1); // 没取走
}

void TestOperationQueue::testDequeueNegative() {
    OperationQueue q;
    q.enqueue(makeOp(1));
    auto ops = q.dequeue(-1);
    QVERIFY(ops.isEmpty());
    QCOMPARE(q.size(), 1);
}

QTEST_MAIN(TestOperationQueue)
#include "TestOperationQueue.moc"
