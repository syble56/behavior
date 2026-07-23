// TestKnobResolver.cpp — 旋钮解析器单元测试
#include <gtest/gtest.h>
#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>

#include "collector/knob_resolver.h"

using namespace ui_shared::behavior;

// ============ KnobResolver 基础解析 ============

TEST(KnobResolver, LeftRotation) {
    KnobResolver::instance().loadDefaults();
    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.action, KnobAction::Left);
}

TEST(KnobResolver, RightRotation) {
    KnobResolver::instance().loadDefaults();
    QKeyEvent event(QEvent::KeyPress, Qt::Key_F12, Qt::ControlModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.action, KnobAction::Right);
}

TEST(KnobResolver, KeySequenceRecorded) {
    KnobResolver::instance().loadDefaults();
    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_TRUE(info.valid);
    EXPECT_FALSE(info.keySequence.isEmpty());
    EXPECT_TRUE(info.keySequence.contains("F11"));
}

TEST(KnobResolver, NotMisidentifyOtherShortcut) {
    KnobResolver::instance().loadDefaults();
    // Ctrl+S 不应识别为旋钮
    QKeyEvent event(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_FALSE(info.valid);
}

TEST(KnobResolver, NotMisidentifyNoModifier) {
    KnobResolver::instance().loadDefaults();
    // 无修饰键的 F11 不应识别为旋钮
    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::NoModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_FALSE(info.valid);
}

TEST(KnobResolver, NotMisidentifyEsc) {
    KnobResolver::instance().loadDefaults();
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_FALSE(info.valid);
}

TEST(KnobResolver, NotMisidentifyEnter) {
    KnobResolver::instance().loadDefaults();
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_FALSE(info.valid);
}

TEST(KnobResolver, ModifierOnlyKeyIgnored) {
    KnobResolver::instance().loadDefaults();
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_FALSE(info.valid);
}

TEST(KnobResolver, NullEventIgnored) {
    KnobResolver::instance().loadDefaults();
    auto info = KnobResolver::instance().resolve(nullptr);
    EXPECT_FALSE(info.valid);
}

// ============ 连续/交替旋转 ============

TEST(KnobResolver, ContinuousLeftRotation) {
    KnobResolver::instance().loadDefaults();
    for (int i = 0; i < 5; ++i) {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier);
        auto info = KnobResolver::instance().resolve(&event);
        EXPECT_TRUE(info.valid) << "Failed at iteration " << i;
        EXPECT_EQ(info.action, KnobAction::Left);
    }
}

TEST(KnobResolver, AlternatingRotation) {
    KnobResolver::instance().loadDefaults();
    for (int i = 0; i < 10; ++i) {
        Qt::Key key = (i % 2 == 0) ? Qt::Key_F11 : Qt::Key_F12;
        KnobAction expected = (i % 2 == 0) ? KnobAction::Left : KnobAction::Right;
        QKeyEvent event(QEvent::KeyPress, key, Qt::ControlModifier);
        auto info = KnobResolver::instance().resolve(&event);
        EXPECT_TRUE(info.valid);
        EXPECT_EQ(info.action, expected) << "Failed at iteration " << i;
    }
}

// ============ 自定义映射 ============

TEST(KnobResolver, CustomMapping) {
    KnobResolver::instance().clear();
    KnobResolver::instance().addMapping(QKeySequence("Ctrl+Up"), KnobAction::Left);
    KnobResolver::instance().addMapping(QKeySequence("Ctrl+Down"), KnobAction::Right);

    QKeyEvent eUp(QEvent::KeyPress, Qt::Key_Up, Qt::ControlModifier);
    auto infoUp = KnobResolver::instance().resolve(&eUp);
    EXPECT_TRUE(infoUp.valid);
    EXPECT_EQ(infoUp.action, KnobAction::Left);

    QKeyEvent eDown(QEvent::KeyPress, Qt::Key_Down, Qt::ControlModifier);
    auto infoDown = KnobResolver::instance().resolve(&eDown);
    EXPECT_TRUE(infoDown.valid);
    EXPECT_EQ(infoDown.action, KnobAction::Right);

    // 恢复默认
    KnobResolver::instance().loadDefaults();
}

TEST(KnobResolver, RemoveMapping) {
    KnobResolver::instance().loadDefaults();
    KnobResolver::instance().removeMapping(QKeySequence(Qt::CTRL | Qt::Key_F11));

    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier);
    auto info = KnobResolver::instance().resolve(&event);
    EXPECT_FALSE(info.valid);  // 已移除，不应再识别

    // F12 仍然有效
    QKeyEvent event2(QEvent::KeyPress, Qt::Key_F12, Qt::ControlModifier);
    auto info2 = KnobResolver::instance().resolve(&event2);
    EXPECT_TRUE(info2.valid);

    KnobResolver::instance().loadDefaults();
}

TEST(KnobResolver, ClearAllMappings) {
    KnobResolver::instance().loadDefaults();
    KnobResolver::instance().clear();

    QKeyEvent e1(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier);
    QKeyEvent e2(QEvent::KeyPress, Qt::Key_F12, Qt::ControlModifier);
    EXPECT_FALSE(KnobResolver::instance().resolve(&e1).valid);
    EXPECT_FALSE(KnobResolver::instance().resolve(&e2).valid);

    KnobResolver::instance().loadDefaults();
}

// ============ 信号测试 ============

TEST(KnobResolver, SignalEmittedOnLeft) {
    KnobResolver::instance().loadDefaults();
    KnobAction receivedAction = KnobAction::Right;  // 初始为不同值
    QObject::connect(&KnobResolver::instance(), &KnobResolver::knobDetected,
        [&receivedAction](KnobAction a) { receivedAction = a; });
    QKeyEvent event(QEvent::KeyPress, Qt::Key_F11, Qt::ControlModifier);
    KnobResolver::instance().resolve(&event);
    EXPECT_EQ(receivedAction, KnobAction::Left);
    QObject::disconnect(&KnobResolver::instance(), &KnobResolver::knobDetected, nullptr, nullptr);
}

TEST(KnobResolver, SignalEmittedOnRight) {
    KnobResolver::instance().loadDefaults();
    KnobAction receivedAction = KnobAction::Left;
    QObject::connect(&KnobResolver::instance(), &KnobResolver::knobDetected,
        [&receivedAction](KnobAction a) { receivedAction = a; });
    QKeyEvent event(QEvent::KeyPress, Qt::Key_F12, Qt::ControlModifier);
    KnobResolver::instance().resolve(&event);
    EXPECT_EQ(receivedAction, KnobAction::Right);
    QObject::disconnect(&KnobResolver::instance(), &KnobResolver::knobDetected, nullptr, nullptr);
}

TEST(KnobResolver, SignalNotEmittedForNonKnob) {
    KnobResolver::instance().loadDefaults();
    bool signalReceived = false;
    QObject::connect(&KnobResolver::instance(), &KnobResolver::knobDetected,
        [&signalReceived](KnobAction) { signalReceived = true; });
    QKeyEvent event(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier);
    KnobResolver::instance().resolve(&event);
    EXPECT_FALSE(signalReceived);
    QObject::disconnect(&KnobResolver::instance(), &KnobResolver::knobDetected, nullptr, nullptr);
}

// ============ 枚举转字符串 ============

TEST(KnobResolver, ActionToString) {
    EXPECT_STREQ(knobActionToString(KnobAction::Left), "knob_left");
    EXPECT_STREQ(knobActionToString(KnobAction::Right), "knob_right");
}
