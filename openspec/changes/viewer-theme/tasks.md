# Tasks

## 1. Shared palette core (platform-agnostic, done first)

- [ ] 1.1 `src/viewer_settings.h`: add a `Palette` struct (slots per design D3) with `kLightPalette`/`kDarkPalette` static tables; add `activePalette()` (lazy, function-local static resolved once per process honoring `activeTheme()` + per-slot INI overrides) and `setHostDark(bool)`/`hostDark()` for the Win32 host flag. Replace usages of `kBackgroundColor`/`kSidebarBackground` everywhere; keep parsing those INI keys as per-slot overrides layered over the active palette.
- [ ] 1.2 `src/wlxplugin.h`: add `#define lcp_darkmode 0x8000` alongside the existing `lcp_*` flags.
- [ ] 1.3 `src/plugin.cpp` + `pluginconfig.h`: thread `lcp_darkmode` (ShowFlags) from all four load entry points on Windows into `viewer_settings::setHostDark`; on Linux leave the host flag unset (Qt scheme drives auto). Wire through `viewer_settings::activeTheme()` for the Theme key.
- [ ] 1.4 `src/pluginconfig.cpp`: parse `[Viewer] Theme` key (`light`/`dark`/`auto`, case-insensitive, default `auto`; malformed → `auto`).
- [ ] 1.5 `assets/multidocviewer.ini` (+ any docs template): document the `Theme` key; adjust comments for `BackgroundColor`/`SidebarBackground` to "per-slot override of the active theme".

## 2. Windows chrome

- [ ] 2.1 `src/viewer_win32.cpp`: page-area fill → `activePalette().pageBg`; selection/search overlay constants (kSelectionFill, search active fill/pen) → palette slots.
- [ ] 2.2 `src/toolbar.cpp` + `src/toolbar_win32.cpp`: toolbar strip background brush + checked tint/ring + glyph color → palette slots; keep `sidebarBackgroundBrush` pattern (process-lifetime, created lazily from frozen palette); add edit-field brushes for `WM_CTLCOLOREDIT`/`WM_CTLCOLORSTATIC` (editBg/editText).
- [ ] 2.3 `src/toolbar_icons.cpp`/`toolbar_icons.h`: glyph color → `activePalette().glyph` (drop `kGlyphColor` hardcode).
- [ ] 2.4 `src/sidebar_win32.cpp`: background brush + tree text color → palette slots (sidebarBg/treeText).
- [ ] 2.5 Rebuild and run the Win32 harness (harness-refit or a light smoke) — verify the toolbar is still drawn and light palette unchanged.

## 3. Linux chrome

- [ ] 3.1 `src/viewer.cpp` (Qt viewer): page fill + selection/search overlay → palette slots.
- [ ] 3.2 `src/toolbar_qt.cpp` (+ shared `src/toolbar.cpp` where glyph/checked colors are used): apply palette to toolbar strip, checked tint/ring, glyph, edit fields (use `QPalette` mappings — Button/Window→toolbarBg, Base→editBg, Text→editText, Highlight→checkedTint/selected items).
- [ ] 3.3 `src/sidebar_qt.cpp`: sidebar background + tree text → palette slots.
- [ ] 3.4 Build with the Linux preset and run a Qt smoke check that sets the host dark flag (or a DARK palette) and asserts the active palette flips.

## 4. Verification harness

- [ ] 4.1 Add `tests/harness_theme.cpp`: headless harness that (a) parses `Theme`/`BackgroundColor`/`SidebarBackground` from a temp INI, (b) calls `setHostDark(true/false)`, (c) asserts `activePalette()` slot values match the light/dark tables and that overrides layer correctly.
- [ ] 4.2 Wire `harness-theme` into `CMakeLists.txt` (pattern after existing `harness-*` targets) and build+run on Windows (where the Qt/Linux target is unavailable, run the pure-palette assertions).
- [ ] 4.3 Confirm no regressions: run existing harnesses (harness-refit, harness-scroll) after the palette refactor.

## 5. Docs wrap-up

- [ ] 5.1 Sync `openspec/changes/viewer-theme/design.md` with any decisions that changed during implementation.
- [ ] 5.2 Mark the theme change complete; archive per OpenSpec workflow.
