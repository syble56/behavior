#pragma once

#include <QSqlDatabase>
#include <QMutex>
#include <QString>
#include <QList>
#include <QSet>
#include "core/Types.h"

namespace ui_shared {
namespace behavior {

class Database {
public:
    static Database& instance();

    // 打开（主线程调用一次）：建立路径，写入schema与索引
    bool open(const QString& path);
    void close();
    bool isOpen() const;

    // 当前线程专属连接（SQLite连接不可跨线程复用）
    // 首次调用时以 m_path 打开并建表/建索引
    QSqlDatabase connection();

    // 插入
    bool insertOperation(const Operation& op);
    bool batchInsert(const QList<Operation>& ops);

    // 查询
    QList<Operation> queryOperations(const QueryFilter& filter);
    qint64 countOperations(const QueryFilter& filter);

    // 会话
    bool insertSession(const Session& session);
    bool updateSession(const Session& session);

    // 清理过期数据，返回删除条数
    int cleanOldData(int retentionDays);

    // 导出原始数据
    QList<Operation> exportRawData(const QueryFilter& filter);

private:
    Database();
    ~Database();
    Q_DISABLE_COPY(Database)

    bool createTables(QSqlDatabase& db);
    bool createIndexes(QSqlDatabase& db);

    QString m_path;
    mutable QMutex m_initMutex;
    QSet<QString> m_openedConnections;  // 已建好的连接名
    bool m_open = false;
};

} // namespace behavior
} // namespace ui_shared
