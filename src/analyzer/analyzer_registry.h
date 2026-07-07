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
    QMap<QString, AnalyzerBase*> analyzers_;
};

} // namespace behavior
} // namespace ui_shared
