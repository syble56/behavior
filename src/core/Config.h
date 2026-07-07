#pragma once

#include <QString>
#include <QStringList>

namespace ui_shared {
namespace behavior {

class Config {
public:
    static Config& instance();

    int retentionDays() const { return retentionDays_; }
    void setRetentionDays(int days) { retentionDays_ = days; }

    int batchSize() const { return batchSize_; }
    void setBatchSize(int size) { batchSize_ = size; }

    int batchTimeoutMs() const { return batchTimeoutMs_; }
    void setBatchTimeoutMs(int ms) { batchTimeoutMs_ = ms; }

    const QStringList& ignoreControls() const { return ignoreControls_; }
    void addIgnoreControl(const QString& className);
    void removeIgnoreControl(const QString& className);
    bool shouldIgnore(const QString& className) const;

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    // 数据库路径（空表示用默认路径）
    QString databasePath() const { return databasePath_; }
    void setDatabasePath(const QString& path) { databasePath_ = path; }

private:
    Config();

    int retentionDays_ = 180;
    int batchSize_ = 100;
    int batchTimeoutMs_ = 1000;
    QStringList ignoreControls_;
    bool enabled_ = true;
    QString databasePath_;
};

} // namespace behavior
} // namespace ui_shared
