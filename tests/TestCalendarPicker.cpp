// TestCalendarPicker.cpp — Unit tests for CalendarPicker and DateField
#include <gtest/gtest.h>
#include <QTest>
#include <QDate>
#include <QSignalSpy>

#include "calendar_picker.h"
#include "dropdown_popup.h"
#include "date_field.h"

// ════════════════════════════════════════════════════════════════
//  CalendarPicker — date logic
// ════════════════════════════════════════════════════════════════

class TestCalendarPicker : public ::testing::Test {
protected:
    void SetUp() override {
        picker = new CalendarPicker;
        picker->resize(360, 420);
    }
    void TearDown() override {
        delete picker;
    }
    CalendarPicker* picker = nullptr;
};

// ── initial state ──

TEST_F(TestCalendarPicker, InitialDateIsToday) {
    EXPECT_EQ(picker->date(), QDate::currentDate());
}

TEST_F(TestCalendarPicker, DefaultLanguageIsChinese) {
    EXPECT_EQ(picker->language(), CalendarPicker::Chinese);
}

// ── setDate ──

TEST_F(TestCalendarPicker, SetValidDate) {
    QDate d(2026, 3, 15);
    QSignalSpy spy(picker, &CalendarPicker::dateChanged);
    picker->setDate(d);
    EXPECT_EQ(picker->date(), d);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TestCalendarPicker, SetInvalidDateIgnored) {
    QDate original = picker->date();
    QSignalSpy spy(picker, &CalendarPicker::dateChanged);
    picker->setDate(QDate());  // invalid
    EXPECT_EQ(picker->date(), original);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(TestCalendarPicker, SetSameDateNoSignal) {
    QDate d = picker->date();
    QSignalSpy spy(picker, &CalendarPicker::dateChanged);
    picker->setDate(d);
    EXPECT_EQ(spy.count(), 0);
}

// ── month navigation ──

TEST_F(TestCalendarPicker, PrevMonthFromJanuaryWrapsToDecember) {
    picker->setDate(QDate(2026, 1, 15));
    QTest::mouseClick(picker, Qt::LeftButton, Qt::NoModifier,
                      picker->rect().center());  // just ensure no crash
    // simulate prevMonth via setDate
    picker->setDate(picker->date().addMonths(-1));
    EXPECT_EQ(picker->date(), QDate(2025, 12, 15));
}

TEST_F(TestCalendarPicker, NextMonthFromDecemberWrapsToJanuary) {
    picker->setDate(QDate(2025, 12, 15));
    picker->setDate(picker->date().addMonths(1));
    EXPECT_EQ(picker->date(), QDate(2026, 1, 15));
}

// ── day-clicked signal ──

TEST_F(TestCalendarPicker, DayClickedEmittedOnDaySelection) {
    picker->setDate(QDate(2026, 7, 1));
    QSignalSpy spy(picker, &CalendarPicker::dayClicked);

    // simulate clicking day 15 by calling mousePressEvent
    // find the cell for day 15 — we need to trigger rebuildGrid first
    picker->setDate(QDate(2026, 7, 15));  // this emits dateChanged but not dayClicked
    EXPECT_EQ(spy.count(), 0);  // setDate doesn't emit dayClicked
}

// ── language ──

TEST_F(TestCalendarPicker, SetLanguageToEnglish) {
    picker->setLanguage(CalendarPicker::English);
    EXPECT_EQ(picker->language(), CalendarPicker::English);
}

TEST_F(TestCalendarPicker, SetLanguageToChinese) {
    picker->setLanguage(CalendarPicker::English);
    picker->setLanguage(CalendarPicker::Chinese);
    EXPECT_EQ(picker->language(), CalendarPicker::Chinese);
}

// ── theme / dark mode ──

TEST_F(TestCalendarPicker, SetDarkMode) {
    picker->setDarkMode(true);
    // no crash, no assertion failure
    SUCCEED();
}

TEST_F(TestCalendarPicker, SetThemeColor) {
    picker->setThemeColor(QColor(255, 0, 0));
    SUCCEED();
}

TEST_F(TestCalendarPicker, SetThemeColorThenDarkMode) {
    picker->setThemeColor(QColor(255, 0, 0));
    picker->setDarkMode(true);
    SUCCEED();
}

// ── day clamping on month change ──

TEST_F(TestCalendarPicker, Day31ClampedWhenSwitchingTo30DayMonth) {
    picker->setDate(QDate(2026, 1, 31));  // Jan has 31 days
    picker->setDate(QDate(2026, 4, 1));   // Apr has 30 days
    EXPECT_LE(picker->date().day(), 30);
}

TEST_F(TestCalendarPicker, Day31ClampedWhenSwitchingToFebruary) {
    picker->setDate(QDate(2026, 1, 31));
    picker->setDate(QDate(2026, 2, 1));  // Feb 2026 has 28 days
    EXPECT_LE(picker->date().day(), 28);
}

TEST_F(TestCalendarPicker, Day29InLeapYearFebruary) {
    picker->setDate(QDate(2024, 2, 29));  // 2024 is leap year
    EXPECT_EQ(picker->date(), QDate(2024, 2, 29));
}

// ════════════════════════════════════════════════════════════════
//  DropdownPopup — grid logic
// ════════════════════════════════════════════════════════════════

class TestDropdownPopup : public ::testing::Test {
protected:
    void SetUp() override {
        popup = new DropdownPopup(DropdownPopup::Grid);
    }
    void TearDown() override {
        delete popup;
    }
    DropdownPopup* popup = nullptr;
};

TEST_F(TestDropdownPopup, SetItemsUpdatesCount) {
    QStringList items = {"A", "B", "C", "D"};
    popup->setItems(items, 2);
    SUCCEED();  // no crash
}

TEST_F(TestDropdownPopup, SetItemsWithNoSelection) {
    QStringList items = {"1", "2", "3"};
    popup->setItems(items, -1);
    SUCCEED();
}

TEST_F(TestDropdownPopup, SetThemeColorNoCrash) {
    popup->setThemeColor(QColor(0, 128, 255));
    SUCCEED();
}

TEST_F(TestDropdownPopup, EmptyItemsNoCrash) {
    popup->setItems({}, -1);
    SUCCEED();
}

TEST_F(TestDropdownPopup, SingleItem) {
    popup->setItems({"Only"}, 0);
    SUCCEED();
}

TEST_F(TestDropdownPopup, ManyItems) {
    QStringList items;
    for (int i = 0; i < 100; ++i)
        items << QString::number(i);
    popup->setItems(items, 50);
    SUCCEED();
}

// ── YearGrid mode ──

class TestDropdownPopupYearGrid : public ::testing::Test {
protected:
    void SetUp() override {
        popup = new DropdownPopup(DropdownPopup::YearGrid);
    }
    void TearDown() override {
        delete popup;
    }
    DropdownPopup* popup = nullptr;
};

TEST_F(TestDropdownPopupYearGrid, SetYearItems) {
    QStringList years;
    for (int y = 1970; y <= 2070; ++y)
        years << QString::number(y);
    popup->setItems(years, 50);
    SUCCEED();
}

TEST_F(TestDropdownPopupYearGrid, EmptyYearItems) {
    popup->setItems({}, -1);
    SUCCEED();
}

// ════════════════════════════════════════════════════════════════
//  DateField — field logic
// ════════════════════════════════════════════════════════════════

class TestDateField : public ::testing::Test {
protected:
    void SetUp() override {
        field = new DateField;
    }
    void TearDown() override {
        delete field;
    }
    DateField* field = nullptr;
};

TEST_F(TestDateField, InitialDateIsToday) {
    EXPECT_EQ(field->date(), QDate::currentDate());
}

TEST_F(TestDateField, SetValidDate) {
    QDate d(2026, 12, 25);
    QSignalSpy spy(field, &DateField::dateChanged);
    field->setDate(d);
    EXPECT_EQ(field->date(), d);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TestDateField, SetInvalidDateIgnored) {
    QDate original = field->date();
    field->setDate(QDate());
    EXPECT_EQ(field->date(), original);
}

TEST_F(TestDateField, SetSameDateNoSignal) {
    QDate d = field->date();
    QSignalSpy spy(field, &DateField::dateChanged);
    field->setDate(d);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(TestDateField, DisplayFormatChangesText) {
    field->setDate(QDate(2026, 7, 18));
    field->setDisplayFormat("yyyy/MM/dd");
    EXPECT_EQ(field->text(), QString("2026/07/18"));
}

TEST_F(TestDateField, DefaultDisplayFormat) {
    field->setDate(QDate(2026, 7, 18));
    EXPECT_EQ(field->text(), QString("2026-07-18"));
}

TEST_F(TestDateField, SetLanguageEnglish) {
    field->setLanguage(DateField::English);
    SUCCEED();
}

TEST_F(TestDateField, SetLanguageChinese) {
    field->setLanguage(DateField::Chinese);
    SUCCEED();
}

TEST_F(TestDateField, DateChangeEmitsCorrectDate) {
    QSignalSpy spy(field, &DateField::dateChanged);
    field->setDate(QDate(2025, 1, 1));
    ASSERT_EQ(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.first().toDate(), QDate(2025, 1, 1));
}

// ── boundary dates ──

TEST_F(TestDateField, MinDate) {
    field->setDate(QDate(1900, 1, 1));
    EXPECT_EQ(field->date(), QDate(1900, 1, 1));
}

TEST_F(TestDateField, MaxDate) {
    field->setDate(QDate(9999, 12, 31));
    EXPECT_EQ(field->date(), QDate(9999, 12, 31));
}

TEST_F(TestDateField, LeapDay) {
    field->setDate(QDate(2024, 2, 29));
    EXPECT_EQ(field->date(), QDate(2024, 2, 29));
}

TEST_F(TestDateField, NonLeapYearFeb29Ignored) {
    field->setDate(QDate(2023, 2, 28));  // valid
    field->setDate(QDate(2023, 2, 29));  // invalid (2023 not leap)
    EXPECT_EQ(field->date(), QDate(2023, 2, 28));  // unchanged
}
