#include "Migrator.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace ui_shared {
namespace behavior {

int Migrator::getVersion(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec("SELECT value FROM metadata WHERE key='version'")) return 0;
    if (q.next()) return q.value(0).toInt();
    return 0;
}

bool Migrator::setVersion(QSqlDatabase& db, int version) {
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO metadata (key,value) VALUES ('version',?)");
    q.addBindValue(version);
    return q.exec();
}

bool Migrator::migrateV1toV2(QSqlDatabase& db) {
    // v1->v2: 本期schema由createTables直接建立，无历史v1库需迁移
    // 预留：若检测到旧表结构，在此执行ALTER
    Q_UNUSED(db)
    return true;
}

bool Migrator::migrate(QSqlDatabase& db) {
    int v = getVersion(db);
    if (v == 0) {
        // 全新库，直接置为当前版本
        return setVersion(db, CURRENT_VERSION);
    }
    if (v < 2) {
        if (!migrateV1toV2(db)) return false;
    }
    return setVersion(db, CURRENT_VERSION);
}

} // namespace behavior
} // namespace ui_shared
