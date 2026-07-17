#include "config.h"

namespace ui_shared {
namespace behavior {

Config& Config::instance() {
    static Config inst;
    return inst;
}

Config::Config() = default;

void Config::addIgnoreControl(const QString& className) {
    if (!ignoreControls_.contains(className)) {
        ignoreControls_.append(className);
    }
}

void Config::removeIgnoreControl(const QString& className) {
    ignoreControls_.removeAll(className);
}

bool Config::shouldIgnore(const QString& className) const {
    return ignoreControls_.contains(className);
}

} // namespace behavior
} // namespace ui_shared
