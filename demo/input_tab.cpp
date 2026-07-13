#include "input_tab.h"
#include "core/behavior_analytics.h"
#include "analyzer/behavior_analyzer.h"

#include <QVBoxLayout>
#include <QVariantMap>
#include <QVector>
#include <QStringList>

using namespace ui_shared::behavior;

InputTab::InputTab(QWidget* parent) : QWidget(parent) {
    chart_ = new PieChartWidget(this);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(chart_);
}

void InputTab::updateData(const QDateTime& start, const QDateTime& end) {
    QVector<double> inputData;
    QStringList inputLabels;

    auto* analyzer = BehaviorAnalytics::analyzer();
    if (analyzer) {
        auto result = analyzer->analyzeInput(start, end);
        QStringList types = {"mouse", "touch", "keyboard", "scroll", "knob"};
        QStringList labels = {"Mouse", "Touch", "Keyboard", "Scroll", "Knob"};
        for (int i = 0; i < types.size(); ++i) {
            QVariantMap m = result.data[types[i]].toMap();
            double v = m["count"].toDouble();
            if (v > 0) { inputLabels << labels[i]; inputData << v; }
        }
    }
    chart_->setData(inputLabels, inputData);
}
