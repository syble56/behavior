// TestAnalyzerOutput.cpp �?分析器输出结�?+ 值正确性验�?// P0: 验证 analyzeInput / analyzeDialog / analyzeFrequency / analyzeModule 返回�?QVariantMap
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QVariantMap>
#include <QVariantList>

#include "storage/database.h"
#include "analyzer/behavior_analyzer.h"
#include "core/types.h"
#include "core/config.h"
#include <memory>

using namespace ui_shared::behavior;

namespace {
// Create a local analyzer that uses the already-opened database
BehaviorAnalyzer* makeAnalyzer() {
    return new BehaviorAnalyzer();
}
Operation makeOp(qint64 ts, EventType et = EventType::MouseClick,
                 InputMethod im = InputMethod::Mouse,
                 const QString& win = "MainWindow", bool mainWin = true,
                 const QString& action = "", const QString& ctrlName = "btnOk") {
    Operation op;
    op.sessionId = "analyzer-test";
    op.timestamp = ts;
    op.eventType = et;
    op.inputMethod = im;
    op.controlClass = "QPushButton";
    op.controlName = ctrlName;
    op.actionName = action;
    op.windowClass = win;
    op.windowTitle = "Test";
    op.isMainWindow = mainWin;
    op.screenX = 100;
    op.screenY = 200;
    op.heatRegion = 42;
    return op;
}
} // namespace

class TestAnalyzerOutput : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // --- Input analyzer ---
    void testInputAnalyzerStructure();
    void testInputAnalyzerPercentages();
    void testInputAnalyzerEmptyData();

    // --- Frequency analyzer ---
    void testFrequencyAnalyzerTopActions();
    void testFrequencyAnalyzerPercentages();
    void testFrequencyAnalyzerActionKeyFallback();

    // --- Module analyzer ---
    void testModuleAnalyzerExtractName();
    void testModuleAnalyzerCounts();

    // --- Dialog analyzer ---
    void testDialogAnalyzerStructure();
    void testDialogAnalyzerDurationStats();
    void testDialogAnalyzerInstantClose();

private:
    QTemporaryDir* dir_ = nullptr;
    QString path_;
    QDateTime baseTime_;
};

void TestAnalyzerOutput::init() {
    dir_ = new QTemporaryDir;
    path_ = dir_->path() + "/analyzer_test.db";
    Config::instance().setDatabasePath(path_);
    Database::instance().open(path_);
    baseTime_ = QDateTime(QDate(2026, 7, 13), QTime(10, 0, 0));
}

void TestAnalyzerOutput::cleanup() {
    Database::instance().close();
    delete dir_;
    dir_ = nullptr;
}

// ===== Input Analyzer =====

void TestAnalyzerOutput::testInputAnalyzerStructure() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeOp(base,       EventType::MouseClick, InputMethod::Mouse));
    Database::instance().insertOperation(makeOp(base + 100, EventType::MouseClick, InputMethod::Mouse));
    Database::instance().insertOperation(makeOp(base + 200, EventType::MouseClick, InputMethod::Keyboard));
    Database::instance().insertOperation(makeOp(base + 300, EventType::MouseClick, InputMethod::Touch));

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    QDateTime start = baseTime_;
    QDateTime end = baseTime_.addSecs(3600);
    AnalysisResult r = analyzer->analyzeInput(start, end);

    QVERIFY(r.success);
    QVERIFY(r.data.contains("total"));
    QVERIFY(r.data.contains("mouse"));
    QVERIFY(r.data.contains("touch"));
    QVERIFY(r.data.contains("keyboard"));
    QVERIFY(r.data.contains("scroll"));
    QVERIFY(r.data.contains("knob"));

    QCOMPARE(r.data["total"].toInt(), 4);

    auto mouse = r.data["mouse"].toMap();
    QVERIFY(mouse.contains("count"));
    QVERIFY(mouse.contains("percentage"));
    QCOMPARE(mouse["count"].toInt(), 2);
}

void TestAnalyzerOutput::testInputAnalyzerPercentages() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // 2 mouse + 1 keyboard + 1 touch = 4 total
    Database::instance().insertOperation(makeOp(base,       EventType::MouseClick, InputMethod::Mouse));
    Database::instance().insertOperation(makeOp(base + 100, EventType::MouseClick, InputMethod::Mouse));
    Database::instance().insertOperation(makeOp(base + 200, EventType::MouseClick, InputMethod::Keyboard));
    Database::instance().insertOperation(makeOp(base + 300, EventType::MouseClick, InputMethod::Touch));

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    QDateTime start = baseTime_;
    QDateTime end = baseTime_.addSecs(3600);
    AnalysisResult r = analyzer->analyzeInput(start, end);

    QVERIFY(r.success);
    QCOMPARE(r.data["total"].toInt(), 4);

    auto mouse = r.data["mouse"].toMap();
    QCOMPARE(mouse["count"].toInt(), 2);
    QCOMPARE(mouse["percentage"].toDouble(), 50.0);

    auto keyboard = r.data["keyboard"].toMap();
    QCOMPARE(keyboard["count"].toInt(), 1);
    QCOMPARE(keyboard["percentage"].toDouble(), 25.0);

    auto touch = r.data["touch"].toMap();
    QCOMPARE(touch["count"].toInt(), 1);
    QCOMPARE(touch["percentage"].toDouble(), 25.0);

    auto scroll = r.data["scroll"].toMap();
    QCOMPARE(scroll["count"].toInt(), 0);
    QCOMPARE(scroll["percentage"].toDouble(), 0.0);
}

void TestAnalyzerOutput::testInputAnalyzerEmptyData() {
    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    QDateTime start = baseTime_;
    QDateTime end = baseTime_.addSecs(3600);
    AnalysisResult r = analyzer->analyzeInput(start, end);

    QVERIFY(!r.success);
    QVERIFY(!r.error.isEmpty());
}

// ===== Frequency Analyzer =====

void TestAnalyzerOutput::testFrequencyAnalyzerTopActions() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // "save" x3, "open" x2, "close" x1
    for (int i = 0; i < 3; ++i)
        Database::instance().insertOperation(makeOp(base + i*100, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "save"));
    for (int i = 0; i < 2; ++i)
        Database::instance().insertOperation(makeOp(base + 300 + i*100, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "open"));
    Database::instance().insertOperation(makeOp(base + 500, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "close"));

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    QDateTime start = baseTime_;
    QDateTime end = baseTime_.addSecs(3600);
    AnalysisResult r = analyzer->analyzeFrequency(start, end);

    QVERIFY(r.success);
    QCOMPARE(r.data["total_operations"].toInt(), 6);
    QCOMPARE(r.data["unique_actions"].toInt(), 3);

    auto top = r.data["top_actions"].toList();
    QVERIFY(!top.isEmpty());
    auto first = top[0].toMap();
    QCOMPARE(first["action"].toString(), QString("save"));
    QCOMPARE(first["count"].toInt(), 3);
}

void TestAnalyzerOutput::testFrequencyAnalyzerPercentages() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    for (int i = 0; i < 4; ++i)
        Database::instance().insertOperation(makeOp(base + i*100, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "actionA"));
    Database::instance().insertOperation(makeOp(base + 400, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "actionB"));

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    AnalysisResult r = analyzer->analyzeFrequency(baseTime_, baseTime_.addSecs(3600));

    QVERIFY(r.success);
    auto top = r.data["top_actions"].toList();
    auto first = top[0].toMap();
    QCOMPARE(first["action"].toString(), QString("actionA"));
    QCOMPARE(first["count"].toInt(), 4);
    QCOMPARE(first["percentage"].toDouble(), 80.0);  // 4/5 * 100
}

void TestAnalyzerOutput::testFrequencyAnalyzerActionKeyFallback() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // No actionName, should fall back to controlName
    Database::instance().insertOperation(makeOp(base, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "", "btnSave"));
    Database::instance().insertOperation(makeOp(base + 100, EventType::MouseClick, InputMethod::Mouse, "MainWindow", true, "", "btnSave"));

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    AnalysisResult r = analyzer->analyzeFrequency(baseTime_, baseTime_.addSecs(3600));

    QVERIFY(r.success);
    auto top = r.data["top_actions"].toList();
    QVERIFY(!top.isEmpty());
    QCOMPARE(top[0].toMap()["action"].toString(), QString("btnSave"));
    QCOMPARE(top[0].toMap()["count"].toInt(), 2);
}

// ===== Module Analyzer =====

void TestAnalyzerOutput::testModuleAnalyzerExtractName() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // "QMainWindow" -> "Main", "SettingsDialog" -> "Settings"
    Database::instance().insertOperation(makeOp(base,       EventType::MouseClick, InputMethod::Mouse, "QMainWindow"));
    Database::instance().insertOperation(makeOp(base + 100, EventType::MouseClick, InputMethod::Mouse, "SettingsDialog"));

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    AnalysisResult r = analyzer->analyzeModule(baseTime_, baseTime_.addSecs(3600));

    QVERIFY(r.success);
    auto modules = r.data["modules"].toList();
    QVERIFY(modules.size() >= 2);

    // Find "Main" and "Settings" in the results
    QStringList names;
    for (const auto& v : modules) names << v.toMap()["module"].toString();
    QVERIFY(names.contains("Main"));
    QVERIFY(names.contains("Settings"));
}

void TestAnalyzerOutput::testModuleAnalyzerCounts() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Database::instance().insertOperation(makeOp(base,       EventType::MouseClick, InputMethod::Mouse, "QMainWindow"));
    Database::instance().insertOperation(makeOp(base + 100, EventType::MouseClick, InputMethod::Mouse, "QMainWindow"));
    Database::instance().insertOperation(makeOp(base + 200, EventType::MouseClick, InputMethod::Mouse, "OptionsDialog"));

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    AnalysisResult r = analyzer->analyzeModule(baseTime_, baseTime_.addSecs(3600));

    QVERIFY(r.success);
    QCOMPARE(r.data["total_modules"].toInt(), 2);

    auto modules = r.data["modules"].toList();
    auto first = modules[0].toMap();
    QCOMPARE(first["module"].toString(), QString("Main"));
    QCOMPARE(first["count"].toInt(), 2);
    QCOMPARE(first["percentage"].toDouble(), (100.0 * 2 / 3));
}

// ===== Dialog Analyzer =====

void TestAnalyzerOutput::testDialogAnalyzerStructure() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    Operation open = makeOp(base, EventType::DialogOpen, InputMethod::Derived, "SettingsDialog");
    open.windowTitle = "Settings";
    Operation close = makeOp(base + 2000, EventType::DialogClose, InputMethod::Derived, "SettingsDialog");
    close.windowTitle = "Settings";
    close.duration = 2000;
    Database::instance().insertOperation(open);
    Database::instance().insertOperation(close);

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    AnalysisResult r = analyzer->analyzeDialog(baseTime_, baseTime_.addSecs(3600));

    QVERIFY(r.success);
    QVERIFY(r.data.contains("total_opens"));
    QVERIFY(r.data.contains("dialogs"));
    QCOMPARE(r.data["total_opens"].toInt(), 1);

    auto dialogs = r.data["dialogs"].toList();
    QVERIFY(!dialogs.isEmpty());
    auto d = dialogs[0].toMap();
    QVERIFY(d.contains("class"));
    QVERIFY(d.contains("open_count"));
    QVERIFY(d.contains("close_count"));
    QVERIFY(d.contains("avg_duration_ms"));
    QVERIFY(d.contains("median_duration_ms"));
    QVERIFY(d.contains("instant_close_rate"));
}

void TestAnalyzerOutput::testDialogAnalyzerDurationStats() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // Open + close with 3000ms duration
    {
        Operation open = makeOp(base, EventType::DialogOpen, InputMethod::Derived, "Dlg");
        open.windowTitle = "MyDialog";
        Operation close = makeOp(base + 3000, EventType::DialogClose, InputMethod::Derived, "Dlg");
        close.windowTitle = "MyDialog";
        close.duration = 3000;
        Database::instance().insertOperation(open);
        Database::instance().insertOperation(close);
    }
    // Open + close with 1000ms duration
    {
        Operation open = makeOp(base + 10000, EventType::DialogOpen, InputMethod::Derived, "Dlg");
        open.windowTitle = "MyDialog";
        Operation close = makeOp(base + 11000, EventType::DialogClose, InputMethod::Derived, "Dlg");
        close.windowTitle = "MyDialog";
        close.duration = 1000;
        Database::instance().insertOperation(open);
        Database::instance().insertOperation(close);
    }

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    AnalysisResult r = analyzer->analyzeDialog(baseTime_, baseTime_.addSecs(36000));

    QVERIFY(r.success);
    QCOMPARE(r.data["total_opens"].toInt(), 2);
    auto dialogs = r.data["dialogs"].toList();
    QCOMPARE(dialogs.size(), 1);
    auto d = dialogs[0].toMap();
    QCOMPARE(d["open_count"].toInt(), 2);
    QCOMPARE(d["close_count"].toInt(), 2);
    QCOMPARE(d["avg_duration_ms"].toInt(), 2000);  // (3000+1000)/2
    QCOMPARE(d["min_duration_ms"].toInt(), 1000);
    QCOMPARE(d["max_duration_ms"].toInt(), 3000);
    QCOMPARE(d["median_duration_ms"].toInt(), 2000);  // median of [1000, 3000]
}

void TestAnalyzerOutput::testDialogAnalyzerInstantClose() {
    qint64 base = baseTime_.toMSecsSinceEpoch();
    // Instant close (< 500ms)
    {
        Operation open = makeOp(base, EventType::DialogOpen, InputMethod::Derived, "Dlg");
        open.windowTitle = "QuickDlg";
        Operation close = makeOp(base + 200, EventType::DialogClose, InputMethod::Derived, "Dlg");
        close.windowTitle = "QuickDlg";
        close.duration = 200;
        Database::instance().insertOperation(open);
        Database::instance().insertOperation(close);
    }
    // Normal close (2000ms)
    {
        Operation open = makeOp(base + 10000, EventType::DialogOpen, InputMethod::Derived, "Dlg");
        open.windowTitle = "QuickDlg";
        Operation close = makeOp(base + 12000, EventType::DialogClose, InputMethod::Derived, "Dlg");
        close.windowTitle = "QuickDlg";
        close.duration = 2000;
        Database::instance().insertOperation(open);
        Database::instance().insertOperation(close);
    }

    auto analyzer = std::unique_ptr<BehaviorAnalyzer>(makeAnalyzer());
    AnalysisResult r = analyzer->analyzeDialog(baseTime_, baseTime_.addSecs(36000));

    QVERIFY(r.success);
    auto dialogs = r.data["dialogs"].toList();
    QCOMPARE(dialogs.size(), 1);
    auto d = dialogs[0].toMap();
    // 1 out of 2 closes was instant (< 500ms) => 50.0%
    QCOMPARE(d["instant_close_rate"].toString(), QString("50.0%"));
}

QTEST_MAIN(TestAnalyzerOutput)
#include "TestAnalyzerOutput.moc"
