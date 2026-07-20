// TestCore.cpp �?补充核心模块测试：Config, Session, DialogTracker, ShortcutResolver
#include <gtest/gtest.h>
#include <QApplication>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QTimer>
#include <QEventLoop>

#include "core/config.h"
#include "core/session.h"
#include "core/types.h"
#include "collector/dialog_tracker.h"
#include "collector/shortcut_resolver.h"

using namespace ui_shared::behavior;

// QApplication fixture for tests that need Qt GUI
// QApplication is created in test_main.cpp

// ============ Config ============

TEST(Config, AddIgnoreControl) {
    auto& cfg = Config::instance();
    cfg.addIgnoreControl("QLabel");
    EXPECT_TRUE(cfg.shouldIgnore("QLabel"));
    cfg.removeIgnoreControl("QLabel");
}

TEST(Config, RemoveIgnoreControl) {
    auto& cfg = Config::instance();
    cfg.addIgnoreControl("QFrame");
    EXPECT_TRUE(cfg.shouldIgnore("QFrame"));
    cfg.removeIgnoreControl("QFrame");
    EXPECT_FALSE(cfg.shouldIgnore("QFrame"));
}

TEST(Config, ShouldNotIgnoreUnregistered) {
    auto& cfg = Config::instance();
    EXPECT_FALSE(cfg.shouldIgnore("QPushButton"));
}

TEST(Config, DuplicateAddIgnored) {
    auto& cfg = Config::instance();
    cfg.addIgnoreControl("QTestWidget");
    cfg.addIgnoreControl("QTestWidget"); // duplicate
    cfg.removeIgnoreControl("QTestWidget");
    EXPECT_FALSE(cfg.shouldIgnore("QTestWidget"));
}

// ============ SessionManager ============

TEST(SessionManager, StartGeneratesId) {
    SessionManager sm;
    QString id = sm.start();
    EXPECT_FALSE(id.isEmpty());
    EXPECT_EQ(id.length(), 36); // UUID without braces
}

TEST(SessionManager, IsActiveAfterStart) {
    SessionManager sm;
    sm.start();
    EXPECT_TRUE(sm.isActive());
}

TEST(SessionManager, NotActiveBeforeStart) {
    SessionManager sm;
    EXPECT_FALSE(sm.isActive());
}

TEST(SessionManager, EndSetsEndTime) {
    SessionManager sm;
    sm.start();
    QThread::msleep(1100);
    auto s = sm.end();
    EXPECT_FALSE(sm.isActive());
    EXPECT_GT(s.endTime, s.startTime);
    EXPECT_GE(s.durationSeconds, 1); // slept >1s
}

TEST(SessionManager, EndWhenNotActive) {
    SessionManager sm;
    auto s = sm.end();
    EXPECT_FALSE(sm.isActive());
    // Returns default session, endTime should be 0
    EXPECT_EQ(s.endTime, 0);
}

TEST(SessionManager, GenerateIdUnique) {
    QString id1 = SessionManager::generateId();
    QString id2 = SessionManager::generateId();
    EXPECT_NE(id1, id2);
}

// ============ DialogTracker ============

TEST(DialogTracker, DialogId) {
    QWidget w;
    w.setObjectName("myDialog");
    QString id = DialogTracker::dialogId(&w);
    EXPECT_TRUE(id.contains("/myDialog"));
    EXPECT_TRUE(id.contains("QWidget"));
}

TEST(DialogTracker, DialogIdNull) {
    QString id = DialogTracker::dialogId(nullptr);
    EXPECT_TRUE(id.isEmpty());
}

TEST(DialogTracker, OpenCloseReturnsDuration) {
    DialogTracker tracker;
    QWidget w;
    w.setObjectName("settingsDlg");
    tracker.onDialogOpen(&w);
    QThread::msleep(50);
    int duration = tracker.onDialogClose(&w);
    EXPECT_GE(duration, 40); // at least ~50ms
}

TEST(DialogTracker, CloseWithoutOpenReturnsZero) {
    DialogTracker tracker;
    QWidget w;
    w.setObjectName("unopenedDlg");
    int duration = tracker.onDialogClose(&w);
    EXPECT_EQ(duration, 0);
}

TEST(DialogTracker, MultipleDialogsTracked) {
    DialogTracker tracker;
    QWidget w1, w2;
    w1.setObjectName("dlg1");
    w2.setObjectName("dlg2");
    tracker.onDialogOpen(&w1);
    tracker.onDialogOpen(&w2);
    QThread::msleep(20);
    int d1 = tracker.onDialogClose(&w1);
    int d2 = tracker.onDialogClose(&w2);
    EXPECT_GE(d1, 15);
    EXPECT_GE(d2, 15);
}

// ============ ShortcutResolver ============

TEST(ShortcutResolver, ResolveNullTarget) {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_S, Qt::NoModifier);
    auto info = ShortcutResolver::resolve(nullptr, &event);
    EXPECT_FALSE(info.valid);
}

TEST(ShortcutResolver, ResolveNullEvent) {
    QWidget w;
    auto info = ShortcutResolver::resolve(&w, nullptr);
    EXPECT_FALSE(info.valid);
}

TEST(ShortcutResolver, ResolveModifierOnlyKey) {
    QWidget w;
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
    auto info = ShortcutResolver::resolve(&w, &event);
    EXPECT_FALSE(info.valid);
}

TEST(ShortcutResolver, ResolveSimpleKey) {
    QWidget w;
    QKeyEvent event(QEvent::KeyPress, Qt::Key_S, Qt::NoModifier);
    auto info = ShortcutResolver::resolve(&w, &event);
    EXPECT_TRUE(info.valid);
    EXPECT_FALSE(info.keySequence.isEmpty());
}

TEST(ShortcutResolver, ResolveWithAction) {
    QWidget w;
    QAction action("Save");
    action.setShortcut(QKeySequence("Ctrl+S"));
    w.addAction(&action);
    QKeyEvent event(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier);
    auto info = ShortcutResolver::resolve(&w, &event);
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.actionName.toStdString(), "Save");
}
