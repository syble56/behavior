#include "database.h"
#include "migrator.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QVariant>
#include <QCoreApplication>
#include <QtGlobal>
#include <QDebug>

namespace ui_shared {
namespace behavior {

namespace {
QString connectionNameForThread() {
    return QStringLiteral("behavior_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

QString defaultPath() {
    QString p = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (p.isEmpty())
        p = QCoreApplication::applicationDirPath();
    QDir d(p);
    if (!d.exists()) d.mkpath(".");
    return p + "/behavior.db";
}
} // namespace

Database& Database::instance() {
    static Database inst;
    return inst;
}

Database::Database() = default;
Database::~Database() { close(); }

bool Database::open(const QString& path) {
    QMutexLocker locker(&initMutex_);
    path_ = path.isEmpty() ? defaultPath() : path;
    qDebug("[Database] Path: %s", qPrintable(path_));
    
    // 直接在主线程创建连接（不调用connection()避免死锁）
    QString name = connectionNameForThread();
    if (QSqlDatabase::contains(name))
        QSqlDatabase::removeDatabase(name);
    
    qDebug("[Database] Creating connection '%s'", qPrintable(name));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    qDebug("[Database] Driver: %s", db.isValid() ? "valid" : "INVALID");
    if (!db.isValid()) {
        qWarning("[Database] QSQLITE driver not available. Drivers: %s", 
                 qPrintable(QSqlDatabase::drivers().join(", ")));
        return false;
    }
    
    db.setDatabaseName(path_);
    qDebug("[Database] Opening: %s", qPrintable(path_));
    if (!db.open()) {
        qWarning("[Database] open failed: %s", qPrintable(db.lastError().text()));
        return false;
    }
    qDebug("[Database] Opened");
    
    // 性能参数
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA synchronous=NORMAL");
    q.exec("PRAGMA busy_timeout=5000");

    if (!createTables(db) || !createIndexes(db)) {
        qWarning("[Database] schema init failed");
        return false;
    }
    Migrator::migrate(db);
    openedConnections_.insert(name);
    open_ = true;
    
    qDebug("[Database] Initialized successfully");
    return true;
}

void Database::close() {
    QMutexLocker locker(&initMutex_);
    for (const QString& name : qAsConst(openedConnections_)) {
        if (QSqlDatabase::contains(name)) {
            QSqlDatabase db = QSqlDatabase::database(name, false);
            if (db.isOpen()) db.close();
            QSqlDatabase::removeDatabase(name);
        }
    }
    openedConnections_.clear();
    open_ = false;
}

bool Database::isOpen() const {
    QMutexLocker locker(&initMutex_);
    return open_;
}

QSqlDatabase Database::connection() {
    QString name = connectionNameForThread();
    if (QSqlDatabase::contains(name)) {
        QSqlDatabase db = QSqlDatabase::database(name, false);
        if (db.isOpen()) return db;
    }
    QMutexLocker locker(&initMutex_);
    if (QSqlDatabase::contains(name))
        QSqlDatabase::removeDatabase(name);
    
    qDebug("[Database] Creating connection '%s'", qPrintable(name));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    qDebug("[Database] Driver: %s", db.isValid() ? "valid" : "INVALID");
    if (!db.isValid()) {
        qWarning("[Database] QSQLITE driver not available. Drivers: %s", 
                 qPrintable(QSqlDatabase::drivers().join(", ")));
        return db;
    }
    
    db.setDatabaseName(path_);
    qDebug("[Database] Opening: %s", qPrintable(path_));
    if (!db.open()) {
        qWarning("[Behavior] Database: open failed: %s",
                 qPrintable(db.lastError().text()));
        return db;
    }
    qDebug("[Database] Opened");
    // 性能参数
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA synchronous=NORMAL");
    q.exec("PRAGMA busy_timeout=5000");

    if (!createTables(db) || !createIndexes(db)) {
        qWarning("[Behavior] Database: schema init failed");
    }
    Migrator::migrate(db);
    openedConnections_.insert(name);
    return db;
}

bool Database::createTables(QSqlDatabase& db) {
    static const char* SQL[] = {
        "CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  id TEXT PRIMARY KEY, start_time INTEGER NOT NULL, end_time INTEGER,"
        "  duration_seconds INTEGER, operation_count INTEGER DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS operations ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, session_id TEXT, time INTEGER NOT NULL,"
        "  event_type TEXT NOT NULL, input_method TEXT,"
        "  control_class TEXT, control_name TEXT, control_text TEXT, control_path TEXT,"
        "  action_name TEXT, key_sequence TEXT,"
        "  window_class TEXT, window_title TEXT, window_path TEXT, is_main_window INTEGER DEFAULT 0,"
        "  module TEXT,"
        "  screen_x INTEGER, screen_y INTEGER, heat_region INTEGER,"
        "  duration INTEGER)",
        "CREATE TABLE IF NOT EXISTS agg_operation_stats ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, time_bucket TEXT NOT NULL, granularity TEXT NOT NULL,"
        "  action_key TEXT NOT NULL, action_type TEXT, count INTEGER NOT NULL,"
        "  UNIQUE(time_bucket, granularity, action_key, action_type))",
        "CREATE TABLE IF NOT EXISTS agg_module_stats ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, time_bucket TEXT NOT NULL, granularity TEXT NOT NULL,"
        "  module_class TEXT NOT NULL, count INTEGER NOT NULL,"
        "  UNIQUE(time_bucket, granularity, module_class))",
        "CREATE TABLE IF NOT EXISTS agg_input_stats ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, time_bucket TEXT NOT NULL, granularity TEXT NOT NULL,"
        "  input_method TEXT NOT NULL, count INTEGER NOT NULL,"
        "  UNIQUE(time_bucket, granularity, input_method))",
        "CREATE TABLE IF NOT EXISTS agg_heatmap_stats ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, time_bucket TEXT NOT NULL, granularity TEXT NOT NULL,"
        "  heat_region INTEGER NOT NULL, count INTEGER NOT NULL,"
        "  UNIQUE(time_bucket, granularity, heat_region))",
        "CREATE TABLE IF NOT EXISTS agg_dialog_stats ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, time_bucket TEXT NOT NULL, granularity TEXT NOT NULL,"
        "  dialog_class TEXT NOT NULL, open_count INTEGER NOT NULL, total_duration INTEGER NOT NULL,"
        "  avg_duration INTEGER, UNIQUE(time_bucket, granularity, dialog_class))",
        "CREATE TABLE IF NOT EXISTS agg_time_distribution ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, date TEXT NOT NULL, hour INTEGER NOT NULL,"
        "  count INTEGER NOT NULL, UNIQUE(date, hour))",
        nullptr
    };
    for (int i = 0; SQL[i]; ++i) {
        QSqlQuery q(db);
        if (!q.exec(SQL[i])) {
            qWarning("[Behavior] create table failed: %s | %s",
                     SQL[i], qPrintable(q.lastError().text()));
            return false;
        }
    }
    return true;
}

bool Database::createIndexes(QSqlDatabase& db) {
    static const char* SQL[] = {
        "CREATE INDEX IF NOT EXISTS idx_ops_time ON operations(time)",
        "CREATE INDEX IF NOT EXISTS idx_ops_session ON operations(session_id)",
        "CREATE INDEX IF NOT EXISTS idx_ops_event ON operations(event_type)",
        "CREATE INDEX IF NOT EXISTS idx_ops_window ON operations(window_class)",
        "CREATE INDEX IF NOT EXISTS idx_ops_mainwin ON operations(is_main_window)",
        "CREATE INDEX IF NOT EXISTS idx_sessions_time ON sessions(start_time)",
        "CREATE INDEX IF NOT EXISTS idx_agg_ops_time ON agg_operation_stats(time_bucket)",
        "CREATE INDEX IF NOT EXISTS idx_agg_ops_action ON agg_operation_stats(action_key)",
        "CREATE INDEX IF NOT EXISTS idx_agg_module_time ON agg_module_stats(time_bucket)",
        nullptr
    };
    for (int i = 0; SQL[i]; ++i) {
        QSqlQuery q(db);
        if (!q.exec(SQL[i])) return false;
    }
    return true;
}

namespace {
void bindOp(QSqlQuery& q, const Operation& op) {
    q.addBindValue(op.sessionId);
    q.addBindValue(op.timestamp);
    q.addBindValue(QString::fromLatin1(eventTypeToString(op.eventType)));
    q.addBindValue(QString::fromLatin1(inputMethodToString(op.inputMethod)));
    q.addBindValue(op.controlClass);
    q.addBindValue(op.controlName);
    q.addBindValue(op.controlText);
    q.addBindValue(op.controlPath);
    q.addBindValue(op.actionName);
    q.addBindValue(op.keySequence);
    q.addBindValue(op.windowClass);
    q.addBindValue(op.windowTitle);
    q.addBindValue(op.windowPath);
    q.addBindValue(op.isMainWindow ? 1 : 0);
    q.addBindValue(op.module);
    q.addBindValue(op.screenX);
    q.addBindValue(op.screenY);
    q.addBindValue(op.heatRegion);
    q.addBindValue(op.duration.has_value() ? QVariant(*op.duration) : QVariant());
}

Operation readOp(const QSqlQuery& q) {
    Operation op;
    op.id = q.value(0).toLongLong();
    op.sessionId = q.value(1).toString();
    op.timestamp = q.value(2).toLongLong();
    op.eventType = stringToEventType(q.value(3).toString());
    op.inputMethod = stringToInputMethod(q.value(4).toString());
    op.controlClass = q.value(5).toString();
    op.controlName = q.value(6).toString();
    op.controlText = q.value(7).toString();
    op.controlPath = q.value(8).toString();
    op.actionName = q.value(9).toString();
    op.keySequence = q.value(10).toString();
    op.windowClass = q.value(11).toString();
    op.windowTitle = q.value(12).toString();
    op.windowPath = q.value(13).toString();
    op.isMainWindow = q.value(14).toInt() == 1;
    op.module = q.value(15).toString();
    op.screenX = q.value(16).toInt();
    op.screenY = q.value(17).toInt();
    op.heatRegion = q.value(18).toInt();
    if (!q.value(19).isNull()) op.duration = q.value(19).toInt();
    return op;
}
} // namespace

bool Database::insertOperation(const Operation& op) {
    return batchInsert({op});
}

bool Database::batchInsert(const QList<Operation>& ops) {
    if (ops.isEmpty()) return true;
    QSqlDatabase db = connection();
    if (!db.isOpen()) return false;
    static const QString SQL = QStringLiteral(
        "INSERT INTO operations (session_id,time,event_type,input_method,"
        "control_class,control_name,control_text,control_path,"
        "action_name,key_sequence,window_class,window_title,window_path,is_main_window,"
        "module,"
        "screen_x,screen_y,heat_region,duration) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    if (!db.transaction()) return false;
    QSqlQuery q(db);
    q.prepare(SQL);
    for (const auto& op : ops) {
        bindOp(q, op);
        if (!q.exec()) {
            db.rollback();
            qWarning("[Behavior] batch insert failed: %s",
                     qPrintable(q.lastError().text()));
            return false;
        }
    }
    return db.commit();
}

QList<Operation> Database::queryOperations(const QueryFilter& filter) {
    QList<Operation> out;
    QSqlDatabase db = connection();
    if (!db.isOpen()) return out;

    QStringList where;
    where << "time >= ?";
    where << "time <= ?";
    if (!filter.sessionId.isEmpty()) where << "session_id = ?";
    if (!filter.eventType.isEmpty()) where << "event_type = ?";
    if (filter.onlyMainWindow)      where << "is_main_window = 1";

    QString sql = "SELECT id,session_id,time,event_type,input_method,"
                  "control_class,control_name,control_text,control_path,"
                  "action_name,key_sequence,window_class,window_title,window_path,is_main_window,"
                  "module,"
                  "screen_x,screen_y,heat_region,duration FROM operations WHERE "
                  + where.join(" AND ") + " ORDER BY time LIMIT ?";

    QSqlQuery q(db);
    q.prepare(sql);
    qint64 s = filter.startTime.toMSecsSinceEpoch();
    qint64 e = filter.endTime.toMSecsSinceEpoch();
    q.addBindValue(s);
    q.addBindValue(e);
    if (!filter.sessionId.isEmpty()) q.addBindValue(filter.sessionId);
    if (!filter.eventType.isEmpty()) q.addBindValue(filter.eventType);
    q.addBindValue(filter.limit);

    if (q.exec()) {
        while (q.next()) out.append(readOp(q));
    } else {
        qWarning("[Behavior] query failed: %s", qPrintable(q.lastError().text()));
    }
    return out;
}

qint64 Database::countOperations(const QueryFilter& filter) {
    QSqlDatabase db = connection();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM operations WHERE time >= ? AND time <= ?");
    q.addBindValue(filter.startTime.toMSecsSinceEpoch());
    q.addBindValue(filter.endTime.toMSecsSinceEpoch());
    if (q.exec() && q.next()) return q.value(0).toLongLong();
    return 0;
}

bool Database::insertSession(const Session& s) {
    QSqlDatabase db = connection();
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO sessions (id,start_time,end_time,duration_seconds,operation_count) "
              "VALUES (?,?,?,?,?)");
    q.addBindValue(s.id);
    q.addBindValue(s.startTime);
    q.addBindValue(s.endTime);
    q.addBindValue(s.durationSeconds);
    q.addBindValue(s.operationCount);
    return q.exec();
}

bool Database::updateSession(const Session& s) {
    QSqlDatabase db = connection();
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.prepare("UPDATE sessions SET end_time=?,duration_seconds=?,operation_count=? WHERE id=?");
    q.addBindValue(s.endTime);
    q.addBindValue(s.durationSeconds);
    q.addBindValue(s.operationCount);
    q.addBindValue(s.id);
    return q.exec();
}

int Database::cleanOldData(int retentionDays) {
    QSqlDatabase db = connection();
    if (!db.isOpen()) return 0;
    qint64 cutoffMs = QDateTime::currentDateTime().addDays(-retentionDays).toMSecsSinceEpoch();
    QSqlQuery q(db);
    q.prepare("DELETE FROM operations WHERE time < ?");
    q.addBindValue(cutoffMs);
    int removed = 0;
    if (q.exec()) removed += q.numRowsAffected();

    // 聚合表清理
    QString cutoffDate = QDateTime::currentDateTime().addDays(-retentionDays).toString("yyyy-MM-dd");
    for (const char* tbl : {"agg_operation_stats","agg_module_stats","agg_input_stats",
                            "agg_heatmap_stats","agg_dialog_stats"}) {
        QSqlQuery qd(db);
        qd.prepare(QStringLiteral("DELETE FROM %1 WHERE time_bucket < ?").arg(QString::fromLatin1(tbl)));
        qd.addBindValue(cutoffDate);
        qd.exec();
    }
    QSqlQuery qt(db);
    qt.prepare("DELETE FROM agg_time_distribution WHERE date < ?");
    qt.addBindValue(cutoffDate);
    qt.exec();
    return removed;
}

QList<Operation> Database::exportRawData(const QueryFilter& filter) {
    return queryOperations(filter);
}

} // namespace behavior
} // namespace ui_shared
