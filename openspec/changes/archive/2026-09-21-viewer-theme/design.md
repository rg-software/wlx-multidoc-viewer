## Context

The chrome is currently a mix of INI-fed constants (`viewer_settings::kBackgroundColor`, `kSidebarBackground`), hardcoded literals duplicated per platform (selection yellow `255,240,105@105`, active-match cyan `0,220,220@150`+`0,130,130`, glyph `0xFF4A4A4A`, checked tint/ring `#E4EFFB`/`#9ABEE0`), and host system colors (Win32 `COLOR_BTNFACE`/`COLOR_WINDOW`; Qt inherits the app palette). The Qt toolbar styles nothing at all. Total Commander signals its mode per load via the standard `lcp_darkmode` bit in each entry point's `ShowFlags`, which `plugin.cpp` currently ignores (`Q_UNUSED(ShowFlags)`). Config is parsed once per process and frozen; Win32 window classes register once with process-lifetime brushes, so a per-process-frozen palette is not just consistent but required. See proposal.md for the why; the requirements are in specs (`viewer-theme`, `plugin-config`).

## Goals / Non-Goals

**Goals:**
- One shared `Palette` struct in `viewer_settings.h` with named slots, consumed by both platform viewers.
- Theme resolution from `[Viewer] Theme` (light/dark/auto), with `auto` following the host: TC flag on Windows, Qt color scheme on Linux.
- Full chrome ownership: no `COLOR_BTNFACE`/`COLOR_WINDOW(...)` dependence for surfaces the palette defines; button/checked glyph and edit-field colors from the palette.
- Resolve once per process, lazily at first viewer construction, so the process-lifetime Win32 class brushes and icon caches stay valid.

**Non-Goals:**
- No page-inversion ("dark reader"): document bitmaps and print output render their own colors (spec: unaffected by theme).
- No live theme switching mid-process; theme is frozen at first resolution (INI + host mode are read-once).
- No per-window theme variance (see Decision D1).
- No CSS/QSS theming — rejected (doesn't reach the Win32 owner-drawn chrome).
- No re-theming of the Qt scrollbar or host-provided widgets.

## Decisions

### D1 — Per-process frozen palette, resolved lazily at first viewer construction

`ViewerSettings`/`viewer_settings::activePalette()` returns a function-local static `const Palette&`, computed on first call. Before that, `plugin.cpp`'s load entry points record the host mode via `viewer_settings::setHostDark(bool)` (`ShowFlags & lcp_darkmode`; called on Windows only — Linux ignores the flag and reads Qt's scheme). Ordering is safe: `ListLoad*` runs before any viewer construction, so the flag is always recorded before the palette is first resolved. All process-lifetime Win32 brushes and per-button icon bitmaps read the same frozen palette.

*Alternatives rejected:* per-window palette resolution — requires moving Win32 sidebar/toolbar backgrounds off the class brushes onto per-window paint paths, with no user-visible benefit (TC mode is uniform within a process). Static-init resolution — impossible on Windows because the `lcp_darkmode` flag is only known at `ListLoad` time.

### D2 — Palette as a plain struct, values sourced from per-theme INI sections

```cpp
struct Palette {
    uint32_t pageBg, sidebarBg, toolbarBg;
    uint32_t toolbarCheckedTint, toolbarCheckedRing;
    uint32_t glyph, treeText, editBg, editText;
    uint32_t selectionFill, searchActiveFill, searchActivePen; // alpha in high byte
};
```

Resolution in `activePalette()`: pick the theme (light/dark, or `auto` → hostDark), start from that theme's **built-in table** (`kLightPalette`/`kDarkPalette`), then overlay the selected theme's INI section — `[Theme:light]` or `[Theme:dark]` — key by key. Each key falls back to the built-in value when missing or malformed, so an absent/partial INI still yields a complete palette. Slot values accept `#RRGGBB` or `#RRGGBBAA` (alpha, repacked to the `0xAARRGGBB` overlay form). The retired `[Viewer] BackgroundColor`/`SidebarBackground` keys are gone; their surfaces are the theme's `PageBackground`/`SidebarBackground` slots.

*Alternatives rejected:* keeping `BackgroundColor`/`SidebarBackground` as extra overrides on top of the theme (two mechanisms for the same surface, and a flat key that cannot express a light/dark pair); CSS/QSS (cannot reach Win32 owner-drawn code); separate theme files (more files to ship/parse than a section).

### D3 — Concrete slot values

| Slot | Light | Dark |
|---|---|---|
| pageBg | `#E8E8E8` | `#1E1E1E` |
| sidebarBg | `#E8E8E8` | `#2B2B2B` |
| toolbarBg | `#F0F0F0` | `#333333` |
| toolbarCheckedTint | `#E4EFFB` | `#2E455E` |
| toolbarCheckedRing | `#9ABEE0` | `#6E94BD` |
| glyph | `#4A4A4A` | `#D6D6D6` |
| treeText | `#202020` | `#E0E0E0` |
| editBg | `#FFFFFF` | `#1F1F1F` |
| editText | `#000000` | `#E0E0E0` |
| selectionFill | `{255,240,105,a105}` | same |
| searchActiveFill | `{0,220,220,a150}` | same |
| searchActivePen | `{0,130,130}` | same |

These are the built-in defaults (design-time values), and the shipped
`multidocviewer.ini` carries them verbatim in `[Theme:light]`/`[Theme:dark]` so
users can tweak any slot. Exact dark hexes are tunable without changing the model.

### D4 — Shared platform-specific code

The palette lives in platform-agnostic `viewer_settings.h`. Application splits by platform:

**Windows (`viewer_win32.cpp`, `viewercontroller.cpp`, `toolbar_win32.cpp`, `toolbar_icons.cpp`, `sidebar_win32.cpp`):**
- Page fill / controller cache slice → `activePalette().pageBg` (same fill code as today, new color source).
- `viewer_win32.cpp` overlay constants `kAr..kAa`/`kCr..kCa` replaced by `activePalette().selectionFill / searchActiveFill / searchActivePen` — removes the cross-platform duplication.
- Toolbar class brush, WM_PAINT strip fill, `drawButton` fill → `toolbarBg`; checked tint/ring → the two checked slots; glyph rasterization (`toolbar_icons.cpp` `kGlyphColor`) → `activePalette().glyph`. All brushes follow the existing process-lifetime factory pattern (`sidebarBackgroundBrush()`), created on first construction from the frozen palette.
- New: the toolbar `wndProc` handles `WM_CTLCOLOREDIT`/`WM_CTLCOLORSTATIC` to paint the page-box/find-box edits and statics from `editBg`/`editText` (currently impossible without this handler — edits follow the system palette today). Same for the sidebar tree: `TreeView_SetBkColor`→`sidebarBg` (already there) plus new `TreeView_SetTextColor`→`treeText`.

**Linux (`viewer.cpp`, `viewercontroller.cpp`, `toolbar_qt.cpp`, `toolbar_icons.cpp`, `sidebar_qt.cpp`):**
- `ViewerCanvas::paintEvent` bg and the controller cache slice → `pageBg`; overlay literals → selection/search slots.
- `ToolbarQt` gets an explicit `QPalette` for the first time: `Window`/`Button`→`toolbarBg`, `Base`→`editBg`, `Text`/`WindowText`/`ButtonText`→`editText`; checked-state emphasis sourced from `Highlight`→`toolbarCheckedTint`. Icons come from `makeIcon` reading `activePalette().glyph`.
- `SidebarQt` additionally sets `Text`→`treeText` on the tree palette (background slots already exist from the sidebar-background change).

Both platforms: shared `naturalSort`/`viewer_settings` unchanged concepts; `kBackgroundColor`/`kSidebarBackground` constants are removed and replaced by `activePalette()` accessors (a small helper preserves any direct callers).

### D5 — Theme key and section parsing

`[Viewer] Theme` parsed case-insensitively with the existing `parseBool`-style helper: `light`, `dark`, or `auto` (default `auto`). `auto` resolves dark when `setHostDark` was recorded (Windows) or Qt's `QStyleHints::colorScheme()` is `Dark` (Linux); otherwise light. Malformed/absent → `auto`.

The selected section (`[Theme:light]`/`[Theme:dark]`) is read with the same `PluginConfig` map access; each slot key is passed through `parseHexColor`, which now accepts 6-digit `#RRGGBB` (opaque) and 8-digit `#RRGGBBAA` (repacked to `0xAARRGGBB`), returning the built-in fallback for anything malformed.

## Risks / Trade-offs

- [Win32 `WM_CTLCOLOR*` is new path] → Brushes created once per process and `DeleteObject` at teardown not required (process-lifetime, like existing sidebar brush); text via `SetBkMode(hDC, TRANSPARENT)` so the static's own background doesn't paint a second color.
- [Retired keys silently ignored] → A user who had `BackgroundColor`/`SidebarBackground` set loses that custom color (it is now ignored). Documented as a migration in the `plugin-config` spec and the INI template; the equivalent value now lives in the theme section.
- [Appearance regression in light mode] → `toolbarBg #F0F0F0` approximates the current `COLOR_BTNFACE`; users on unusual OS themes lose that subtle custom tint. Acceptable within "fully theme-owned" (explicit decision).
- [Qt checked-state rendering differs from Win32 owner-draw] → Semantic parity (checked emphasis from the palette) rather than pixel parity; documented in the spec scenario wording.
- [High-contrast/accessibility OS settings ignored by forced colors] → Pre-existing behavior (page/sidebar already forced); unchanged by this design.

## Migration Plan

- No config migration: `BackgroundColor`/`SidebarBackground` keep working identically in light mode; `Theme` is additive.
- Rollback: revert the change; behavior returns to today's light (system-driven) chrome.

## Open Questions

None — specs, approach, and task breakdown are settled.

## Implementation Notes

- Theme parsing lives in `viewer_settings.h` (`parseTheme`/`activeTheme`), not
  `pluginconfig.cpp`: the INI reader stays a generic structure and the theme is
  a `viewer_settings` concern (the task text named the file, the behavior is
  identical).
- Linux host mode is recorded through the same `setHostDark()` path as Windows,
  read from the Qt color-scheme hint (Qt 6.5+; older Qt falls back to the window
  palette lightness) in `plugin.cpp` right before viewer construction. This keeps
  `viewer_settings.h` free of Qt and still satisfies the "auto follows the Qt
  color-scheme hint" scenario.
- On Win32 the toolbar's STATIC labels blend with the strip (background =
  `toolbarBg`) while the EDIT boxes use `editBg`, both with `editText` — using
  `editBg` for the labels would paint a light box on the strip. The palette still
  owns both surfaces.
- `assets/multidocviewer.ini` and `dist/release/multidocviewer.ini` ship the full
  `[Theme:light]`/`[Theme:dark]` palettes (values `#RRGGBB`/`#RRGGBBAA`).
  `BackgroundColor`/`SidebarBackground` are retired and ignored; their surfaces
  are the theme's `PageBackground`/`SidebarBackground` slots.
- Verification: `harness-theme` (15 checks), `harness-refit`, and `harness-scroll`
  all pass on Windows. `harness-scroll`'s `G` tests post the `B` presentation key
  (the key remap is unrelated to this change); they were updated from the stale
  `P`.