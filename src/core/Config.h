#pragma once

#include <QString>
#include <QStringList>

namespace ui_shared {
namespace behavior {

class Config {
public:
    static Config& instance();

    int retentionDays() const { return m_retentionDays; }
    void setRetentionDays(int days) { m_retentionDays = days; }

    int batchSize() const { return m_batchSize; }
    void setBatchSize(int size) { m_batchSize = size; }

    int batchTimeoutMs() const { return m_batchTimeoutMs; }
    void setBatchTimeoutMs(int ms) { m_batchTimeoutMs = ms; }

    const QStringList& ignoreControls() const { return m_ignoreControls; }
    void addIgnoreControl(const QString& className);
    void removeIgnoreControl(const QString& className);
    bool shouldIgnore(const QString& className) const;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    // 数据库路径（空表示用默认路径）
    QString databasePath() const { return m_databasePath; }
    void setDatabasePath(const QString& path) { m_databasePath = path; }

private:
    Config();

    int m_retentionDays = 180;
    int m_batchSize = 100;
    int m_batchTimeoutMs = 1000;
    QStringList m_ignoreControls;
    bool m_enabled = true;
    QString m_databasePath;
};

} // namespace behavior
} // namespace ui_shared
