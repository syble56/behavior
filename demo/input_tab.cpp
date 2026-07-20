#include "input_tab.h"
#include "chart_widgets.h"
#include "storage/database.h"

#include <QVBoxLayout>
#include <QSqlQuery>
#include <QSqlDatabase>
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
    auto& db = Database::instance();
    QSqlDatabase sqlDb = db.connection();
    if (!sqlDb.isOpen()) return;

    qint64 startMs = start.toMSecsSinceEpoch();
    qint64 endMs = end.toMSecsSinceEpoch();
    QString startBucket = start.toString("yyyy-MM-dd");
    QString endBucket = end.toString("yyyy-MM-dd");
    int rangeDays = start.date().daysTo(end.date()) + 1;

    bool useAgg = (rangeDays > 7);

    QSqlQuery q(sqlDb);
    QVector<double> inputData;
    QStringList inputLabels;

    // Map DB input_method values to display labels
    struct InputEntry { const char* dbValue; const char* label; };
    static const InputEntry entries[] = {
        {"mouse_click", "Mouse Click"},
        {"touch_tap",   "Touch Tap"},
        {"shortcut",    "Keyboard"},
        {"scroll",      "Scroll"},
        {"knob",        "Knob"},
    };

    if (useAgg) {
        q.prepare("SELECT input_method, SUM(count) as cnt FROM agg_input_stats "
                  "WHERE time_bucket >= ? AND time_bucket <= ? GROUP BY input_method ORDER BY cnt DESC");
        q.addBindValue(startBucket); q.addBindValue(endBucket); q.exec();
        while (q.next()) {
            QString method = q.value(0).toString();
            double cnt = q.value(1).toDouble();
            if (cnt <= 0) continue;
            // Find display label
            QString label = method;
            for (auto& e : entries) {
                if (method == e.dbValue) { label = e.label; break; }
            }
            inputLabels << label;
            inputData << cnt;
        }
        if (inputData.isEmpty()) {
            q.prepare("SELECT input_method, COUNT(*) as cnt FROM operations "
                      "WHERE time >= ? AND time < ? AND input_method != 'derived' "
                      "GROUP BY input_method ORDER BY cnt DESC");
            q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
            while (q.next()) {
                QString method = q.value(0).toString();
                double cnt = q.value(1).toDouble();
                if (cnt <= 0) continue;
                QString label = method;
                for (auto& e : entries) {
                    if (method == e.dbValue) { label = e.label; break; }
                }
                inputLabels << label;
                inputData << cnt;
            }
        }
    } else {
        q.prepare("SELECT input_method, COUNT(*) as cnt FROM operations "
                  "WHERE time >= ? AND time < ? AND input_method != 'derived' "
                  "GROUP BY input_method ORDER BY cnt DESC");
        q.addBindValue(startMs); q.addBindValue(endMs); q.exec();
        while (q.next()) {
            QString method = q.value(0).toString();
            double cnt = q.value(1).toDouble();
            if (cnt <= 0) continue;
            QString label = method;
            for (auto& e : entries) {
                if (method == e.dbValue) { label = e.label; break; }
            }
            inputLabels << label;
            inputData << cnt;
        }
    }

    chart_->setData(inputLabels, inputData);
}
