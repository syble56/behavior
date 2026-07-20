// TestAnalyzers.cpp — 补充分析器测试：heatmap, time, registry
#include <gtest/gtest.h>
#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>

#include "analyzer/heatmap_analyzer.h"
#include "analyzer/time_analyzer.h"
#include "analyzer/analyzer_registry.h"
#include "analyzer/frequency_analyzer.h"
#include "analyzer/module_analyzer.h"
#include "analyzer/input_analyzer.h"
#include "core/types.h"

using namespace ui_shared::behavior;

namespace {
Operation makeOp(qint64 ts, bool mainWin = true, int region = 42) {
    Operation op;
    op.sessionId = "s1";
    op.timestamp = ts;
    op.eventType = EventType::MouseClick;
    op.inputMethod = InputMethod::Mouse;
    op.windowClass = "MainWindow";
    op.isMainWindow = mainWin;
    op.heatRegion = region;
    op.controlClass = "QPushButton";
    op.controlName = "btn";
    return op;
}
} // namespace

// ============ HeatmapAnalyzer ============

TEST(HeatmapAnalyzer, EmptyInput) {
    HeatmapAnalyzer a;
    auto r = a.analyze({});
    EXPECT_FALSE(r.success);
    EXPECT_TRUE(r.error.contains("无"));
}

TEST(HeatmapAnalyzer, OnlyNonMainWindow) {
    HeatmapAnalyzer a;
    QList<Operation> ops = { makeOp(1000, false, 5), makeOp(2000, false, 10) };
    auto r = a.analyze(ops);
    EXPECT_FALSE(r.success);
}

TEST(HeatmapAnalyzer, SingleRegion) {
    HeatmapAnalyzer a;
    QList<Operation> ops = { makeOp(1000, true, 45), makeOp(2000, true, 45), makeOp(3000, true, 45) };
    auto r = a.analyze(ops);
    EXPECT_TRUE(r.success);
    auto regions = r.data["regions"].toList();
    EXPECT_EQ(regions.size(), 1);
    EXPECT_EQ(regions[0].toMap()["count"].toInt(), 3);
    EXPECT_EQ(regions[0].toMap()["row"].toInt(), 4);
    EXPECT_EQ(regions[0].toMap()["col"].toInt(), 5);
    EXPECT_EQ(r.data["max_count"].toInt(), 3);
}

TEST(HeatmapAnalyzer, MultipleRegions) {
    HeatmapAnalyzer a;
    QList<Operation> ops = {
        makeOp(1000, true, 0), makeOp(2000, true, 0),
        makeOp(3000, true, 99), makeOp(4000, true, 50),
    };
    auto r = a.analyze(ops);
    EXPECT_TRUE(r.success);
    auto regions = r.data["regions"].toList();
    EXPECT_EQ(regions.size(), 3);
    EXPECT_EQ(r.data["max_count"].toInt(), 2);
}

TEST(HeatmapAnalyzer, FiltersNonMainWindow) {
    HeatmapAnalyzer a;
    QList<Operation> ops = {
        makeOp(1000, true, 10),
        makeOp(2000, false, 20),
        makeOp(3000, true, 10),
    };
    auto r = a.analyze(ops);
    EXPECT_TRUE(r.success);
    auto regions = r.data["regions"].toList();
    EXPECT_EQ(regions.size(), 1);
    EXPECT_EQ(regions[0].toMap()["count"].toInt(), 2);
}

TEST(HeatmapAnalyzer, InvalidRegionIgnored) {
    HeatmapAnalyzer a;
    QList<Operation> ops = {
        makeOp(1000, true, 50),
        makeOp(2000, true, -1),
        makeOp(3000, true, 100),
    };
    auto r = a.analyze(ops);
    EXPECT_TRUE(r.success);
    auto regions = r.data["regions"].toList();
    EXPECT_EQ(regions.size(), 1);
    EXPECT_EQ(regions[0].toMap()["region"].toInt(), 50);
}

// ============ TimeAnalyzer ============

TEST(TimeAnalyzer, EmptyInput) {
    TimeAnalyzer a;
    auto r = a.analyze({});
    EXPECT_FALSE(r.success);
}

TEST(TimeAnalyzer, ByHour) {
    TimeAnalyzer a;
    a.setGranularity(TimeAnalyzer::Granularity::Hour);
    QDateTime base(QDate(2026, 7, 17), QTime(9, 0, 0));
    QList<Operation> ops = {
        makeOp(base.addSecs(0).toMSecsSinceEpoch()),
        makeOp(base.addSecs(3600).toMSecsSinceEpoch()),
        makeOp(base.addSecs(7200).toMSecsSinceEpoch()),
        makeOp(base.addSecs(7200).toMSecsSinceEpoch()),
    };
    auto r = a.analyze(ops);
    EXPECT_TRUE(r.success);
    auto byHour = r.data["by_hour"].toList();
    EXPECT_EQ(byHour.size(), 3);
    // 9: 1, 10: 1, 11: 2
    EXPECT_EQ(byHour[0].toMap()["bucket"].toInt(), 9);
    EXPECT_EQ(byHour[0].toMap()["count"].toInt(), 1);
    EXPECT_EQ(byHour[2].toMap()["count"].toInt(), 2);
}

TEST(TimeAnalyzer, ByDayOfWeek) {
    TimeAnalyzer a;
    a.setGranularity(TimeAnalyzer::Granularity::DayOfWeek);
    QDateTime mon(QDate(2026, 7, 13), QTime(10, 0, 0)); // Monday
    QDateTime tue(QDate(2026, 7, 14), QTime(10, 0, 0)); // Tuesday
    QList<Operation> ops = {
        makeOp(mon.toMSecsSinceEpoch()),
        makeOp(mon.addSecs(60).toMSecsSinceEpoch()),
        makeOp(tue.toMSecsSinceEpoch()),
    };
    auto r = a.analyze(ops);
    EXPECT_TRUE(r.success);
    auto byBucket = r.data["by_bucket"].toList();
    EXPECT_EQ(byBucket.size(), 2);
}

TEST(TimeAnalyzer, PeakBuckets) {
    TimeAnalyzer a;
    a.setGranularity(TimeAnalyzer::Granularity::Hour);
    QDateTime base(QDate(2026, 7, 17), QTime(9, 0, 0));
    QList<Operation> ops;
    for (int i = 0; i < 5; ++i) ops << makeOp(base.toMSecsSinceEpoch());
    for (int i = 0; i < 3; ++i) ops << makeOp(base.addSecs(3600).toMSecsSinceEpoch());
    auto r = a.analyze(ops);
    EXPECT_TRUE(r.success);
    EXPECT_EQ(r.data["max_count"].toInt(), 0); // max_count not set in TimeAnalyzer
    // peak is hour 9
    auto byHour = r.data["by_hour"].toList();
    EXPECT_EQ(byHour[0].toMap()["count"].toInt(), 5);
}

// ============ AnalyzerRegistry ============

TEST(AnalyzerRegistry, RegisterAndGet) {
    auto& reg = AnalyzerRegistry::instance();
    FrequencyAnalyzer fa;
    reg.registerAnalyzer("test_freq", &fa);
    auto* got = reg.get("test_freq");
    EXPECT_EQ(got, &fa);
}

TEST(AnalyzerRegistry, GetNonExistent) {
    auto& reg = AnalyzerRegistry::instance();
    auto* got = reg.get("nonexistent_analyzer");
    EXPECT_EQ(got, nullptr);
}

TEST(AnalyzerRegistry, Names) {
    auto& reg = AnalyzerRegistry::instance();
    ModuleAnalyzer ma;
    reg.registerAnalyzer("test_module", &ma);
    QStringList names = reg.names();
    EXPECT_TRUE(names.contains("test_module"));
}
