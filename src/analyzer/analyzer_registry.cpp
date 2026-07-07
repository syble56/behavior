#include "analyzer_registry.h"
#include "analyzer_base.h"

namespace ui_shared {
namespace behavior {

AnalyzerRegistry& AnalyzerRegistry::instance() {
    static AnalyzerRegistry inst;
    return inst;
}

void AnalyzerRegistry::registerAnalyzer(const QString& name, AnalyzerBase* analyzer) {
    analyzers_[name] = analyzer;
}

AnalyzerBase* AnalyzerRegistry::get(const QString& name) const {
    return analyzers_.value(name);
}

QStringList AnalyzerRegistry::names() const {
    return analyzers_.keys();
}

} // namespace behavior
} // namespace ui_shared
