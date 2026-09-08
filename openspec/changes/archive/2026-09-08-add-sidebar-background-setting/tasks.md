## 1. Shared Setting

- [x] 1.1 Add `kSidebarBackground` to `src/viewer_settings.h`: `inline uint32_t kSidebarBackground = parseHexColor(PluginConfig::get().get("Viewer").get("SidebarBackground"), 0xE8E8E8);` placed next to `kBackgroundColor`, with a comment matching the existing style
- [x] 1.2 Document the new key in `assets/multidocviewer.ini` under `[Viewer]`: `SidebarBackground = #E8E8E8` with a `;` comment explaining it is the sidebar background hex RGB value

## 2. Win32 Sidebar

- [x] 2.1 In `src/sidebar_win32.cpp`, build a process-lifetime `HBRUSH` from `viewer_settings::kSidebarBackground` (function-local `static`, created on first `SidebarWin32` construction, never deleted) and assign it to the `WLX_SIDEBAR_CLASS` background brush (replacing `COLOR_WINDOW + 1`)
- [x] 2.2 Assign the same process-lifetime brush to the `WLX_SIDEBAR_GRIP_CLASS` background (replacing `COLOR_BTNFACE + 1`)
- [x] 2.3 After creating `m_tree`, send `TVM_SETBKCOLOR` (via `TreeView_SetBkColor`) with the configured color so the tree viewport matches the panel background

## 3. Qt Sidebar

- [x] 3.1 In `src/sidebar_qt.cpp`, build `QColor` from `viewer_settings::kSidebarBackground` and set a `QPalette` (`Window` + `Base`) on `m_tree` and `m_tree->viewport()`, with `setAutoFillBackground(true)` on both
- [x] 3.2 Set `QPalette::Window` + `setAutoFillBackground(true)` on the sidebar container `this` (covers the `ResizeGrip` via palette inheritance)

## 4. Verification

- [x] 4.1 Build Windows: `cmake --preset windows-x64-release && cmake --build --preset windows-release` and confirm the plugin links
- [x] 4.2 Build Linux: `cmake --preset linux-release && cmake --build --preset linux-release` and confirm the plugin links
- [x] 4.3 Smoke-test Win32 in Total/Double Commander: open a document with an outline, set `SidebarBackground` to a distinct color in the installed `multidocviewer.ini`, reload, and confirm the sidebar (panel, tree rows, grip) renders that color while the page area keeps its own background; delete the key and confirm the `0xE8E8E8` fallback
- [x] 4.4 Smoke-test Qt sidebar on Linux with the same two INI states (explicit color, absent key) and confirm behavior matches the Win32 side