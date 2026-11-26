// Stand-in used when MicForge is built without LV2 support (MICFORGE_WITH_LV2=OFF, e.g. on Windows).
#include "plugins.h"

namespace mf {

bool lv2Available(QString *why) {
    if (why) *why = "this build of MicForge has no LV2 support";
    return false;
}

std::vector<PluginInfo> scanLv2() { return {}; }

std::unique_ptr<PluginInstance> createLv2(const PluginInfo &, double, int, QString *error) {
    if (error) *error = "can't be loaded: this build of MicForge has no LV2 support";
    return nullptr;
}

} // namespace mf
