#pragma once

#include <QMap>
#include <QString>

namespace ui_shared {
namespace behavior {

class AnalyzerBase;

class AnalyzerRegistry {
public:
    static AnalyzerRegistry& instance();

    void registerAnalyzer(const QString& name, AnalyzerBase* analyzer);
    AnalyzerBase* get(const QString& name) const;
    QStringList names() const;

private:
    QMap<QString, AnalyzerBase*> m_analyzers;
};

} // namespace behavior
} // namespace ui_shared
