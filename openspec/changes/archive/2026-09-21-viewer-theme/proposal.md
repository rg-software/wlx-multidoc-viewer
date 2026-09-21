## Why

The plugin's chrome does not follow the host's theme: Total Commander in dark mode leaves the page area and sidebar light gray, toolbar glyphs dark on a system-colored strip (mixing with a dark OS), and the checked-tint/selection colors are hardcoded and duplicated per platform. The colors also have no structure — `BackgroundColor`/`SidebarBackground` are flat INI keys with no notion of a light vs dark palette.

## What Changes

- **Palette model**: a shared `Palette` struct of named color slots (page background, sidebar background, toolbar background, checked tint/ring, glyph, tree text, edit background/text) with built-in light and dark defaults. The values are codified in the INI as `[Theme:light]` / `[Theme:dark]` sections (hex `#RRGGBB` or `#RRGGBBAA`), so any slot is user-customizable; a missing key falls back to the built-in default. Selection/search overlay colors are centralized (currently duplicated in `viewer_win32.cpp` and `viewer.cpp`) and normally identical in both themes.
- **Theme resolution**: a new `[Viewer] Theme = light | dark | auto` INI key (default `auto`). `auto` follows the host: Windows uses the TC dark-mode flag (`lcp_darkmode` in the `ShowFlags` of every `ListLoad*`/`ListLoadNext*` entry point, newly defined in `wlxplugin.h`); Linux uses the Qt `QStyleHints::colorScheme`. Resolution is lazy, once per process at first viewer construction, frozen thereafter — matching the existing read-once config lifecycle.
- **`BackgroundColor`/`SidebarBackground` are retired**: their surfaces move into the theme as the `PageBackground`/`SidebarBackground` slots (a light/dark pair instead of one flat value). The old keys are ignored.
- **Toolbar becomes fully theme-owned on both platforms** (replaces `COLOR_BTNFACE` on Win32; gives the Qt toolbar an explicit palette instead of inheriting), and Win32 edit/static controls and the sidebar tree text are colored from the palette (new `WM_CTLCOLOR` + `TVM_SETTEXTCOLOR` handling).
- **Not themed**: rendered document pages (documents keep their own colors), the print pipeline (physical output stays white), and the MOBI cover.

## Capabilities

### New Capabilities
- `viewer-theme`: platform-agnostic capability describing how the plugin derives its active palette (host/OS/override sources), the color slots it exposes, and where each slot is applied on both platforms.

### Modified Capabilities
- `plugin-config`: the `BackgroundColor` and `SidebarBackground` requirements change from "the page/sidebar color" to "a per-slot override of the active theme's palette", and a `Theme` key (`light|dark|auto`) is added to `[Viewer]`.

## Impact

- `src/wlxplugin.h`: add `lcp_darkmode 0x8000`.
- `src/plugin.cpp`: thread the dark flag from all four load entry points to the shared open path.
- `src/viewer_settings.h`: `Palette` struct + built-in light/dark tables + lazy resolution from the INI theme sections.
- Paint sites: `viewer_win32.cpp`, `viewer.cpp`, `viewercontroller.cpp`, `toolbar_win32.cpp`, `toolbar_icons.cpp`, `sidebar_win32.cpp`, `sidebar_qt.cpp`, `toolbar_qt.cpp`.
- `assets/multidocviewer.ini` (+ `dist/release`): `[Theme:light]`/`[Theme:dark]` sections; `BackgroundColor`/`SidebarBackground` removed.
- Specs: new `openspec/specs/viewer-theme/spec.md`; delta on `openspec/specs/plugin-config/spec.md` (adds `Theme` + theme sections, removes the two flat color keys).