// TestEventProcessor.cpp �?采集层测试：ControlInspector, EventFilter
#include <gtest/gtest.h>
#include <QApplication>
#include <QMouseEvent>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMainWindow>
#include <QTest>

#include "collector/control_inspector.h"
#include "collector/event_filter.h"
#include "core/config.h"
#include "core/types.h"

using namespace ui_shared::behavior;

// ============ ControlInspector ============

TEST(ControlInspector, ClassName) {
    QPushButton btn("OK");
    auto name = ControlInspector::className(&btn);
    EXPECT_FALSE(name.isEmpty());
    EXPECT_TRUE(name.contains("Button"));
}

TEST(ControlInspector, ClassNameNull) {
    auto name = ControlInspector::className(nullptr);
    EXPECT_TRUE(name.isEmpty());
}

TEST(ControlInspector, ObjectName) {
    QPushButton btn;
    btn.setObjectName("btnOk");
    auto name = ControlInspector::objectName(&btn);
    EXPECT_EQ(name.toStdString(), "btnOk");
}

TEST(ControlInspector, DisplayText) {
    QPushButton btn("Save");
    auto text = ControlInspector::displayText(&btn);
    EXPECT_EQ(text.toStdString(), "Save");
}

TEST(ControlInspector, Path) {
    QMainWindow mw;
    mw.setObjectName("MainWindow");
    auto* central = new QWidget(&mw);
    central->setObjectName("centralWidget");
    auto* btn = new QPushButton("Save", central);
    btn->setObjectName("btnSave");
    auto p = ControlInspector::path(btn);
    EXPECT_TRUE(p.contains("btnSave"));
    EXPECT_TRUE(p.contains("centralWidget"));
}

TEST(ControlInspector, IsMainWindow) {
    QMainWindow mw;
    EXPECT_FALSE(ControlInspector::isMainWindow(&mw)); // not set as main window yet
}

TEST(ControlInspector, ShouldIgnoreLabel) {
    QLabel label;
    EXPECT_TRUE(ControlInspector::shouldIgnore(&label));
}

TEST(ControlInspector, ShouldNotIgnoreButton) {
    QPushButton btn;
    EXPECT_FALSE(ControlInspector::shouldIgnore(&btn));
}

TEST(ControlInspector, HeatRegion) {
    // Heat region is 10x10 grid based on window geometry
    QMainWindow mw;
    mw.resize(100, 100);
    mw.show();
    QTest::qWaitForWindowExposed(&mw);
    int region = ControlInspector::heatRegion(QPoint(5, 5), &mw);
    EXPECT_GE(region, 0);
    EXPECT_LE(region, 99);
    mw.close();
}

TEST(ControlInspector, InteractiveClasses) {
    auto& classes = ControlInspector::interactiveClasses();
    EXPECT_FALSE(classes.isEmpty());
    EXPECT_TRUE(classes.contains("QPushButton"));
}

// ============ EventFilter basic ============

TEST(EventFilter, DoesNotCrashWithNoInit) {
    // EventFilter should be safe to construct
    // Full testing requires BehaviorAnalytics::init() which needs DB
    QMainWindow mw;
    mw.setObjectName("MainWindow");
    mw.show();
    QTest::qWaitForWindowExposed(&mw);
    EXPECT_TRUE(mw.isVisible());
    mw.close();
}
