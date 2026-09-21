# Tasks

## 1. Shared palette core (platform-agnostic, done first)

- [x] 1.1 `src/viewer_settings.h`: add a `Palette` struct (slots per design D3) with built-in `kLightPalette`/`kDarkPalette` defaults; extend `parseHexColor` to accept `#RRGGBB` and `#RRGGBBAA` (repacking alpha to `0xAARRGGBB`); add `activePalette()` (lazy, function-local static resolved once per process) that starts from the active theme's built-in table and overlays the selected `[Theme:light]`/`[Theme:dark]` section, key by key, falling back to the built-in default for any missing/malformed key; add `setHostDark(bool)`/`hostDark()`.
- [x] 1.2 `src/wlxplugin.h`: add `#define lcp_darkmode 0x8000` alongside the existing `lcp_*` flags.
- [x] 1.3 `src/plugin.cpp`: thread the host mode from all four load entry points into `viewer_settings::setHostDark` (Windows: `ShowFlags & lcp_darkmode`; Linux: the Qt color-scheme hint, recorded just before viewer construction).
- [x] 1.4 `viewer_settings.h`: parse `[Viewer] Theme` (`light`/`dark`/`auto`, case-insensitive, default `auto`; malformed → `auto`) in `activeTheme()`.
- [x] 1.5 `assets/multidocviewer.ini` + `dist/release/multidocviewer.ini`: add the `[Theme:light]`/`[Theme:dark]` sections with all 12 slots (hex `#RRGGBB`/`#RRGGBBAA`); remove the retired `BackgroundColor`/`SidebarBackground` keys.

## 2. Windows chrome

- [x] 2.1 `src/viewer_win32.cpp`: page-area fill → `activePalette().pageBg`; selection/search overlay constants → palette slots.
- [x] 2.2 `src/toolbar.cpp` + `src/toolbar_win32.cpp`: toolbar strip background brush + checked tint/ring + glyph color → palette slots; process-lifetime brushes; add edit-field brushes for `WM_CTLCOLOREDIT`/`WM_CTLCOLORSTATIC`.
- [x] 2.3 `src/toolbar_icons.cpp`/`toolbar_icons.h`: glyph color → `activePalette().glyph` (drop `kGlyphColor` hardcode).
- [x] 2.4 `src/sidebar_win32.cpp`: background brush + tree text color → palette slots (sidebarBg/treeText).
- [x] 2.5 Rebuild and run the Win32 harness — plugin + harness-refit + harness-scroll build; harness-refit ALL PASS (see 4.3 for the harness-scroll caveat).

## 3. Linux chrome

- [x] 3.1 `src/viewer.cpp` (Qt viewer): page fill + selection/search overlay → palette slots.
- [x] 3.2 `src/toolbar_qt.cpp`: apply palette to toolbar strip, checked tint/ring, glyph, edit fields via `QPalette` mappings.
- [x] 3.3 `src/sidebar_qt.cpp`: sidebar background + tree text → palette slots.
- [ ] 3.4 Build with the Linux preset and run a Qt smoke check that sets the host dark flag (or a DARK palette) and asserts the active palette flips. **Not run — no Linux toolchain in this environment (Windows-only). Requires `cmake --preset linux-release` + `harness-theme`.**

## 4. Verification harness

- [x] 4.1 Add `tests/harness_theme.cpp`: headless harness writing a temp INI per scenario, calling `setHostDark(true/false)`, asserting `activePalette()` slots (light/dark/auto/malformed, theme-section values, 8-digit alpha, missing-key fallback, section selection, precedence).
- [x] 4.2 Wire `harness-theme` into `CMakeLists.txt` (built + run on Windows: all 15 checks pass, exit 0).
- [x] 4.3 Confirm no regressions: run existing harnesses (harness-refit, harness-scroll). **harness-refit ALL PASS. harness-scroll's `G` tests were updated to the intended `B` presentation key and now report ALL PASS; the failures were the stale `P` key, not the theme change (proved by stashing all edits and rebuilding at HEAD, then restoring only `P`).**

## 5. Docs wrap-up

- [x] 5.1 Sync `openspec/changes/viewer-theme/design.md`, `proposal.md`, and the specs with the INI-codified palette and the retired keys.
- [ ] 5.2 Mark the theme change complete; archive per OpenSpec workflow.
