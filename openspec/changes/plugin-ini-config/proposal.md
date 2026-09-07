## Why

All UI tuning lives in compile-time constants (`viewer_settings.h` says "no runtime config by design"). Users who want to change the page-area background color must edit source and rebuild. Introducing a thin INI layer lets the same binary adapt to different environments without code changes, and gives future settings (sidebar width default, scroll step, etc.) a ready home.

## What Changes

- The plugin reads `multidocviewer.ini` from its own directory (next to the DLL) on first access via the **mINI** header-only library (v0.9.14, `pulzed-mini` vcpkg port on Windows, fetched at configure time on Linux — matching the approach in `wlx-edge-viewer`). The parse is lazy, cached in a process-lifetime `static mINI::INIStructure`, and never written back at runtime.
- `ListSetDefaultParams` remains a no-op; the plugin resolves the INI path itself from the module location (`GetModuleHandle`/`GetModuleFileName` on Windows, `dladdr` on Linux), exactly as `wlx-edge-viewer` does.
- A new shared header (`src/pluginconfig.h`) exposes `PluginConfig::get()` returning the cached INI structure, and `PluginConfig::modulePath()` returning the plugin directory.
- The first consumer is the **page-area background color**: `viewer_settings.h` gains a runtime `kBackgroundColor` populated from `[Viewer] BackgroundColor` (hex, e.g. `#E8E8E8` or `E8E8E8`; default `0xE8E8E8`). The three existing hardcoded sites (`viewer_win32.cpp`, `viewer.cpp`, `viewercontroller.cpp`) are consolidated through this single value.
- A shipped `multidocviewer.ini` template (next to the DLL / installed alongside the `.wlx64`) documents the available keys.

## Capabilities

### New Capabilities
- `plugin-config`: INI file location and format, lazy load/cache semantics, `GetModulePath` resolution, the `[Viewer]` section and its `BackgroundColor` key.

### Modified Capabilities
- none

## Impact

- New dependency: `pulzed-mini` (mINI) via `vcpkg.json` on Windows; fetched by `CMakeLists.txt` on Linux.
- New files: `src/pluginconfig.h` (singleton + path), `assets/multidocviewer.ini` (shipped template).
- Modified files: `vcpkg.json` (+`pulzed-mini`), `CMakeLists.txt` (Linux fetch), `src/plugin.cpp` (`ListSetDefaultParams` stub stays but `pluginconfig.h` is included), `src/viewer_settings.h` (runtime background color), `src/viewer_win32.cpp`, `src/viewer.cpp`, `src/viewercontroller.cpp` (background color read from config instead of hardcoded).