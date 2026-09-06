#ifndef PLUGINCONFIG_H
#define PLUGINCONFIG_H

#include <mini/ini.h>
#include <string>

namespace PluginConfig {

// Lazily parsed, process-lifetime cached INI structure read from
// <module directory>/multidocviewer.ini. Never written back at runtime.
const mINI::INIStructure& get();

// Directory containing the loaded plugin binary (DLL on Windows, .so on
// Linux). No trailing separator.
std::string modulePath();

} // namespace PluginConfig

#endif // PLUGINCONFIG_H