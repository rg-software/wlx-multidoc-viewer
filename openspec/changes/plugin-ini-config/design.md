## Context

See `proposal.md` — Why/What. The sibling project `wlx-edge-viewer` already ships a proven INI layer using mINI v0.9.14, lazy-loaded from a file next to the DLL, with `ListSetDefaultParams` as a no-op. This change replicates that architecture in `wlx-multidoc-viewer` and validates it with a first consumer (background color).

## Goals / Non-Goals

**Goals:**
- Ship a `multidocviewer.ini` file next to the plugin binary on both platforms.
- Parse it once, lazily, cache the result for the process lifetime.
- Expose a `PluginConfig::get()` singleton accessible from any translation unit.
- Consolidate the hardcoded background color `0xE8E8E8` (currently in three files) into a single config-driven value with a documented hex format and a safe fallback.

**Non-Goals:**
- Writing back to the INI at runtime (read-only, same as `wlx-edge-viewer`).
- Using the host's `DefaultIniName` (`ListSetDefaultParams` stays a no-op).
- Exposing every `viewer_settings.h` constant through the INI (future work; only background color is in scope).
- Dark-mode or auto-detection of the background color.

## Decisions

**D1 — Use mINI v0.9.14 via `pulzed-mini` vcpkg port on Windows, fetched at configure time on Linux.**
This is the exact same dual-pin strategy used in `wlx-edge-viewer`. On Windows the port is declared in `vcpkg.json`; on Linux the `CMakeLists.txt` downloads the single header from the pinned commit. Both pins reference the same tag so behavior is identical.
*Alternatives rejected:* `QSettings` (pulls Qt INI parser, not header-only, couples to Qt for a non-Qt subsystem); hand-rolled parser (mINI is battle-tested and 789 lines of header).

**D2 — Resolve the INI path from the loaded module, not from `DefaultIniName`.**
Windows: `GetModuleHandleW(nullptr)` → `GetModuleFileNameW` → strip to directory. Linux: `dladdr` on a function in the plugin → `dli_fname` → `parent_path()`. This matches the `wlx-edge-viewer` approach and ensures the INI is always next to the DLL regardless of which host loads the plugin.
*Alternatives rejected:* using `dps->DefaultIniName` (host-dependent, may point to `wincmd.ini` which the plugin should not write to).

**D3 — Singleton with static local, lazy init guarded by `std::call_once` or empty-structure check.**
The `mINI::INIStructure` is returned by reference from `PluginConfig::get()`. First call reads the file (or uses an empty structure); subsequent calls are a single branch (structure size > 0). Thread safety is not required because `ListSetDefaultParams` is always called from the host's main thread before any viewer is created.
*Alternatives rejected:* global variable with explicit init (requires init-order coordination); `Q_GLOBAL_STATIC` (Qt-specific, couples to Qt).

**D4 — Background color is a `COLORREF` (Win32) / `QColor` (Qt) derived from a single `QRgb` in the config.**
`viewer_settings.h` exposes `inline QRgb kBackgroundColor` initialized from the config singleton. On Win32 the viewer converts `QRgb` → `COLORREF`; on Qt it wraps as `QColor`. The hex parser strips an optional `#` prefix, validates 6 hex digits, and falls back to `0xE8E8E8` on any parse failure. The three existing fill sites (`viewer_win32.cpp:473`, `viewer.cpp:23`, `viewercontroller.cpp:452`) all read `viewer_settings::kBackgroundColor` instead of hardcoding.
*Alternatives rejected:* per-platform color constants (drift); passing color through the viewer constructor (would require plumbing changes).

**D5 — Ship a template `multidocviewer.ini` in `Resources/` installed alongside the plugin.**
The template documents `[Viewer] BackgroundColor` with a commented example. On Windows it sits next to the `.wlx64`; on Linux it is installed to the same `share/doublecmd/plugins/multidoc/` directory. If the user deletes the template, defaults apply.

**D6 — INI section naming: `[Viewer]` for page-area settings.**
Future sections (`[Sidebar]`, `[Toolbar]`) can be added later. The section name is short, matches the feature area, and is consistent with `wlx-edge-viewer`'s section-per-feature convention.

## Platform-Specific Code

- **Windows (`src/pluginconfig.cpp`, `src/viewer_win32.cpp`):** `GetModuleHandleW(nullptr)` + `GetModuleFileNameW` to resolve the module directory; `mINI::INIFile` with UTF-8 path; background color converted from `QRgb` to `COLORREF` (`RGB(r, g, b)`) for `CreateSolidBrush`.
- **Linux (`src/pluginconfig.cpp`, `src/viewer.cpp`):** `dladdr` to resolve the `.so` directory; `mINI::INIFile` with UTF-8 path; background color used directly as `QColor(QRgb)` in `paintEvent`.
- **Shared (`src/pluginconfig.h`, `src/viewer_settings.h`):** `PluginConfig::get()` and `modulePath()` are platform-agnostic; `kBackgroundColor` is a single `QRgb` read from the config on both platforms.

## Risks / Trade-offs

- [First access triggers file I/O on the main thread] → The INI is a few hundred bytes at most; the cost is negligible compared to the document open that follows. Same trade-off accepted in `wlx-edge-viewer`.
- [User edits the INI while the plugin is running] → Not observed (cached). This is documented behavior and matches `wlx-edge-viewer`. A restart is required to pick up changes.
- [INI file encoding] → mINI expects UTF-8 or ASCII. The template is pure ASCII. Non-ASCII values in other keys would need UTF-8 BOM handling, but `BackgroundColor` is hex-only, so this is not a concern for the first consumer.
- [Build-system dual-pin drift] → Both the vcpkg baseline and the Linux `MINI_COMMIT` must reference the same mINI tag. This constraint is documented in the tasks and mirrors the existing `wlx-edge-viewer` AGENTS.md convention.