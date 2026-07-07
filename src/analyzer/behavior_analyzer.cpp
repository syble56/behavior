#include "behavior_analyzer.h"
#include "analyzer_base.h"
#include "analyzer_registry.h"
#include "frequency_analyzer.h"
#include "module_analyzer.h"
#include "time_analyzer.h"
#include "input_analyzer.h"
#include "heatmap_analyzer.h"
#include "dialog_analyzer.h"
#include "storage/database.h"

namespace ui_shared {
namespace behavior {

BehaviorAnalyzer::BehaviorAnalyzer(QObject* parent)
    : QObject(parent),
      frequency_(std::make_unique<FrequencyAnalyzer>()),
      module_(std::make_unique<ModuleAnalyzer>()),
      time_(std::make_unique<TimeAnalyzer>()),
      input_(std::make_unique<InputAnalyzer>()),
      heatmap_(std::make_unique<HeatmapAnalyzer>()),
      dialog_(std::make_unique<DialogAnalyzer>()) {}

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
    if (name == "frequency") a = frequency_.get();
    else if (name == "module")   a = module_.get();
    else if (name == "time")     a = time_.get();
    else if (name == "input")    a = input_.get();
    else if (name == "heatmap")  a = heatmap_.get();
    else if (name == "dialog")   a = dialog_.get();
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
