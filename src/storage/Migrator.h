#pragma once

#include <QSqlDatabase>

namespace ui_shared {
namespace behavior {

class Migrator {
public:
    static constexpr int CURRENT_VERSION = 2;

    static bool migrate(QSqlDatabase& db);

    static int getVersion(QSqlDatabase& db);
    static bool setVersion(QSqlDatabase& db, int version);

private:
    static bool migrateV1toV2(QSqlDatabase& db);
};

} // namespace behavior
} // namespace ui_shared
