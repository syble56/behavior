// TestEventCollection.cpp �?事件采集层测试：EventProcessor 按键/旋钮/鼠标
#include <gtest/gtest.h>
#include <QApplication>
#include <QKeyEvent>
#include <QPoint>
#include <QThread>
#include <QWidget>
#include <QPushButton>
#include <QMainWindow>
#include <QTest>

#include "collector/event_processor.h"
#include "collector/knob_resolver.h"
#include "storage/operation_queue.h"
#include "core/config.h"
#include "core/types.h"

using namespace ui_shared::behavior;

// ============ EventProcessor 按键测试 ============

TEST(EventProcessor, EscKeyPressRecorded) {
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("test-session");

    QMainWindow mw;
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    processor.processKeyPress(&mw, &event);

    ASSERT_EQ(queue.size(), 1);
    auto ops = queue.dequeue(1);
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].eventType, EventType::Shortcut);
    EXPECT_EQ(ops[0].inputMethod, InputMethod::Keyboard);
    EXPECT_TRUE(ops[0].actionName.contains("escape", Qt::CaseInsensitive));
    mw.close();
}

TEST(EventProcessor, EnterKeyPressRecorded) {
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("test-session");

    QMainWindow mw;
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    processor.processKeyPress(&mw, &event);

    ASSERT_EQ(queue.size(), 1);
    auto ops = queue.dequeue(1);
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].eventType, EventType::Shortcut);
    EXPECT_EQ(ops[0].inputMethod, InputMethod::Keyboard);
    EXPECT_TRUE(ops[0].actionName.contains("enter", Qt::CaseInsensitive));
    mw.close();
}

TEST(EventProcessor, ShortcutWithModifierRecorded) {
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("test-session");

    QMainWindow mw;
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    // Ctrl+S 应记录为 shortcut
    QKeyEvent event(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier);
    processor.processShortcut(&mw, &event);

    ASSERT_EQ(queue.size(), 1);
    auto ops = queue.dequeue(1);
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].eventType, EventType::Shortcut);
    EXPECT_EQ(ops[0].inputMethod, InputMethod::Keyboard);
    EXPECT_FALSE(ops[0].keySequence.isEmpty());
    mw.close();
}

// ============ EventProcessor 旋钮测试 ============

TEST(EventProcessor, KnobLeftRecorded) {
    KnobResolver::instance().loadDefaults();
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("test-session");

    QMainWindow mw;
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier);
    processor.processKnob(&mw, &event);

    ASSERT_EQ(queue.size(), 1);
    auto ops = queue.dequeue(1);
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].eventType, EventType::Knob);
    EXPECT_EQ(ops[0].inputMethod, InputMethod::Knob);
    EXPECT_EQ(ops[0].actionName.toStdString(), "knob_left");
    EXPECT_FALSE(ops[0].keySequence.isEmpty());
    mw.close();
}

TEST(EventProcessor, KnobRightRecorded) {
    KnobResolver::instance().loadDefaults();
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("test-session");

    QMainWindow mw;
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    QKeyEvent event(QEvent::KeyPress, Qt::Key_F12, Qt::ControlModifier);
    processor.processKnob(&mw, &event);

    ASSERT_EQ(queue.size(), 1);
    auto ops = queue.dequeue(1);
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].eventType, EventType::Knob);
    EXPECT_EQ(ops[0].inputMethod, InputMethod::Knob);
    EXPECT_EQ(ops[0].actionName.toStdString(), "knob_right");
    mw.close();
}

TEST(EventProcessor, KnobNotEnqueuedForNonKnobKey) {
    KnobResolver::instance().loadDefaults();
    OperationQueue queue;
    EventProcessor processor(&queue);

    QMainWindow mw;
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    // Ctrl+S 不是旋钮，processKnob 不应入队
    QKeyEvent event(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier);
    processor.processKnob(&mw, &event);

    EXPECT_EQ(queue.size(), 0);
    mw.close();
}

TEST(EventProcessor, KnobSessionIdAssociated) {
    KnobResolver::instance().loadDefaults();
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("my-knob-session");

    QMainWindow mw;
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier);
    processor.processKnob(&mw, &event);

    auto ops = queue.dequeue(1);
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].sessionId.toStdString(), "my-knob-session");
    mw.close();
}

// ============ EventProcessor 鼠标点击测试 ============

TEST(EventProcessor, MouseClickRecorded) {
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("test-session");

    QMainWindow mw;
    mw.resize(200, 200);
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    QPoint pos = mw.geometry().center();
    processor.processMouseClick(&mw, pos);

    ASSERT_EQ(queue.size(), 1);
    auto ops = queue.dequeue(1);
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].eventType, EventType::MouseClick);
    EXPECT_EQ(ops[0].inputMethod, InputMethod::Mouse);
    EXPECT_GT(ops[0].screenX, 0);
    EXPECT_GT(ops[0].screenY, 0);
    mw.close();
}

TEST(EventProcessor, MouseClickDedup) {
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("test-session");

    QMainWindow mw;
    mw.resize(200, 200);
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    QPoint pos = mw.geometry().center();
    processor.processMouseClick(&mw, pos);
    // 同位置立即再点（<50ms），应去�?    processor.processMouseClick(&mw, pos);

    EXPECT_EQ(queue.size(), 1);
    mw.close();
}

// ============ EventProcessor 对话框测�?============

TEST(EventProcessor, DialogOpenCloseRecorded) {
    OperationQueue queue;
    EventProcessor processor(&queue);
    processor.setSessionId("test-session");

    QWidget dlg;
    dlg.setObjectName("testDialog");

    processor.processDialogOpen(&dlg);
    QThread::msleep(50);
    processor.processDialogClose(&dlg);

    auto ops = queue.dequeue(10);
    ASSERT_EQ(ops.size(), 2);
    EXPECT_EQ(ops[0].eventType, EventType::DialogOpen);
    EXPECT_EQ(ops[1].eventType, EventType::DialogClose);
    EXPECT_GE(ops[1].duration, 40);
}

