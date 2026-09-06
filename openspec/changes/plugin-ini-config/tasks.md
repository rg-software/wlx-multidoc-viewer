## 1. mINI dependency and build wiring

- [ ] 1.1 Add `"pulzed-mini"` to `vcpkg.json` dependencies (Windows: header-only via vcpkg manifest).
- [ ] 1.2 In `CMakeLists.txt`, add the Linux-only `file(DOWNLOAD ...)` block for mINI v0.9.14 (commit `a1ff72e8898db8b53282e9eb7c7ec5973519787e`, SHA256 `1396DB49DD4EEC37E5556341989AAEB6256FC44349D7BCB8F3E1581F277C44A3`) into the build directory, and add `${CMAKE_CURRENT_BINARY_DIR}/mini` to the target include path (mirroring `wlx-edge-viewer/CMakeLists.txt` lines 16–32).

## 2. Config singleton and module path

- [ ] 2.1 Create `src/pluginconfig.h`: declare `namespace PluginConfig { const mINI::INIStructure& get(); std::string modulePath(); }`. Include `<mini/ini.h>` and `<string>`.
- [ ] 2.2 Create `src/pluginconfig.cpp` (platform-split with `#ifdef _WIN32`): implement `get()` as a static-local `mINI::INIStructure` lazily read from `modulePath() + "/multidocviewer.ini"`; implement `modulePath()` using `GetModuleHandleW(nullptr)` + `GetModuleFileNameW` on Windows and `dladdr` on Linux.
- [ ] 2.3 Create `Resources/multidocviewer.ini` template with a commented `[Viewer] BackgroundColor` key and a brief header comment.

## 3. Background color consumer

- [ ] 3.1 Add a hex-parsing helper (internal linkage or `PluginConfig` namespace) that strips an optional `#` prefix, validates 6 hex digits, and returns a `uint32_t` RGB or a fallback value.
- [ ] 3.2 In `src/viewer_settings.h`, add `inline uint32_t kBackgroundColor` initialized from `PluginConfig::get()["Viewer"]["BackgroundColor"]` via the hex parser, falling back to `0xE8E8E8`. Remove the `"no runtime config by design"` comment.
- [ ] 3.3 In `src/viewer_win32.cpp`: remove `constexpr COLORREF kBgColor = 0xE8E8E8;` (line 63); replace `CreateSolidBrush(kBgColor)` in `onPaint()` with `CreateSolidBrush(RGB((viewer_settings::kBackgroundColor >> 16) & 0xFF, (viewer_settings::kBackgroundColor >> 8) & 0xFF, viewer_settings::kBackgroundColor & 0xFF))`.
- [ ] 3.4 In `src/viewer.cpp`: replace `QColor(0xE8, 0xE8, 0xE8)` in `ViewerCanvas::paintEvent` with `QColor::fromRgb(viewer_settings::kBackgroundColor)`.
- [ ] 3.5 In `src/viewercontroller.cpp`: replace `slice.fill(0xE8E8E8)` in `renderCachedViewport` with `slice.fill(viewer_settings::kBackgroundColor)`.

## 4. Build verification

- [ ] 4.1 Build the Windows preset (`cmake --preset windows-x64-release && cmake --build --preset windows-release`); confirm clean compile and `multidocviewer.ini` is present next to the output DLL.
- [ ] 4.2 Build the Linux preset (`cmake --preset linux-release && cmake --build --preset linux-release`); confirm clean compile and `multidocviewer.ini` is fetched into the build directory.
- [ ] 4.3 Smoke test: load a document with no INI file present (default gray background); create a `multidocviewer.ini` with `[Viewer] BackgroundColor=#FFFFFF`, reload (new viewer window), confirm white background; test malformed value (`BackgroundColor=red`) falls back to gray.