#include "AnalyzerRegistry.h"
#include "AnalyzerBase.h"

namespace ui_shared {
namespace behavior {

AnalyzerRegistry& AnalyzerRegistry::instance() {
    static AnalyzerRegistry inst;
    return inst;
}

void AnalyzerRegistry::registerAnalyzer(const QString& name, AnalyzerBase* analyzer) {
    m_analyzers[name] = analyzer;
}

AnalyzerBase* AnalyzerRegistry::get(const QString& name) const {
    return m_analyzers.value(name);
}

QStringList AnalyzerRegistry::names() const {
    return m_analyzers.keys();
}

} // namespace behavior
} // namespace ui_shared
