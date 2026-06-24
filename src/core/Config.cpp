#include "Config.h"

namespace ui_shared {
namespace behavior {

Config& Config::instance() {
    static Config inst;
    return inst;
}

Config::Config() = default;

void Config::addIgnoreControl(const QString& className) {
    if (!m_ignoreControls.contains(className))
        m_ignoreControls.append(className);
}

void Config::removeIgnoreControl(const QString& className) {
    m_ignoreControls.removeAll(className);
}

bool Config::shouldIgnore(const QString& className) const {
    return m_ignoreControls.contains(className);
}

} // namespace behavior
} // namespace ui_shared
