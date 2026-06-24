#include "BehaviorAnalyzer.h"
#include "AnalyzerBase.h"
#include "AnalyzerRegistry.h"
#include "FrequencyAnalyzer.h"
#include "ModuleAnalyzer.h"
#include "TimeAnalyzer.h"
#include "InputAnalyzer.h"
#include "HeatmapAnalyzer.h"
#include "DialogAnalyzer.h"
#include "storage/Database.h"

namespace ui_shared {
namespace behavior {

BehaviorAnalyzer::BehaviorAnalyzer(QObject* parent)
    : QObject(parent),
      m_frequency(std::make_unique<FrequencyAnalyzer>()),
      m_module(std::make_unique<ModuleAnalyzer>()),
      m_time(std::make_unique<TimeAnalyzer>()),
      m_input(std::make_unique<InputAnalyzer>()),
      m_heatmap(std::make_unique<HeatmapAnalyzer>()),
      m_dialog(std::make_unique<DialogAnalyzer>()) {}

BehaviorAnalyzer::~BehaviorAnalyzer() = default;

QList<Operation> BehaviorAnalyzer::fetchData(const QDateTime& start, const QDateTime& end,
                                             bool onlyMainWindow) {
    QueryFilter f;
    f.startTime = start;
    f.endTime = end;
    f.onlyMainWindow = onlyMainWindow;
    f.limit = 1000000;
    return Database::instance().queryOperations(f);
}

AnalysisResult BehaviorAnalyzer::run(const QString& name, const QDateTime& start, const QDateTime& end,
                                     bool onlyMainWindow) {
    QList<Operation> ops = fetchData(start, end, onlyMainWindow);
    AnalyzerBase* a = nullptr;
    if (name == "frequency") a = m_frequency.get();
    else if (name == "module")   a = m_module.get();
    else if (name == "time")     a = m_time.get();
    else if (name == "input")    a = m_input.get();
    else if (name == "heatmap")  a = m_heatmap.get();
    else if (name == "dialog")   a = m_dialog.get();
    else a = AnalyzerRegistry::instance().get(name);

    if (!a) {
        AnalysisResult r;
        r.error = QStringLiteral("未知分析器: %1").arg(name);
        return r;
    }
    a->setTimeRange(start, end);
    return a->analyze(ops);
}

AnalysisResult BehaviorAnalyzer::analyzeFrequency(const QDateTime& s, const QDateTime& e) { return run("frequency", s, e); }
AnalysisResult BehaviorAnalyzer::analyzeModule(const QDateTime& s, const QDateTime& e)   { return run("module", s, e); }
AnalysisResult BehaviorAnalyzer::analyzeTime(const QDateTime& s, const QDateTime& e)     { return run("time", s, e); }
AnalysisResult BehaviorAnalyzer::analyzeInput(const QDateTime& s, const QDateTime& e)    { return run("input", s, e); }
AnalysisResult BehaviorAnalyzer::analyzeHeatmap(const QDateTime& s, const QDateTime& e)  { return run("heatmap", s, e, true); }
AnalysisResult BehaviorAnalyzer::analyzeDialog(const QDateTime& s, const QDateTime& e)   { return run("dialog", s, e); }

AnalysisResult BehaviorAnalyzer::analyze(const QString& name, const QDateTime& s, const QDateTime& e) {
    return run(name, s, e);
}

QList<Operation> BehaviorAnalyzer::exportRawData(const QDateTime& start, const QDateTime& end) {
    return fetchData(start, end, false);
}

} // namespace behavior
} // namespace ui_shared
