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

### D2 — Palette as a plain struct with explicit light/dark tables + INI override layering

```cpp
struct Palette {
    uint32_t pageBg, sidebarBg, toolbarBg;
    uint32_t toolbarCheckedTint, toolbarCheckedRing;
    uint32_t glyph, treeText, editBg, editText;
    uint32_t selectionFill, searchActiveFill, searchActivePen; // same in both themes
};
```

Resolution in `activePalette()`: pick base table by theme (light/dark, or `auto` → hostDark), then stack the legacy `[Viewer] BackgroundColor` / `SidebarBackground` keys as per-slot overrides (they keep their current parse rules and defaults disappear behind the theme default). Overrides always win over the theme default for their slot, matching the spec ("overrides always win").

*Alternatives rejected:* per-slot-per-theme INI keys (e.g. `BackgroundColor.Dark`) — flat, no code-side palette object, more INI surface; CSS/QSS — cannot reach Win32 owner-drawn code, would fork the styling into two mechanisms.

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

These are design-time values; exact dark hexes are tunable without changing the model.

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

### D5 — Theme key parsing

`[Viewer] Theme` parsed case-insensitively with the existing `parseBool`-style helper: `light`, `dark`, or `auto` (default `auto`). `auto` resolves dark when `setHostDark` was recorded (Windows) or Qt's `QStyleHints::colorScheme()` is `Dark` (Linux); otherwise light. Malformed/absent → `auto`.

## Risks / Trade-offs

- [Win32 `WM_CTLCOLOR*` is new path] → Brushes created once per process and `DeleteObject` at teardown not required (process-lifetime, like existing sidebar brush); text via `SetBkMode(hDC, TRANSPARENT)` so the static's own background doesn't paint a second color.
- [Overrides can paper over dark mode] → A user with `BackgroundColor="#E8E8E8"` set gets a light page even in dark theme. Intended ("overrides always win", spec), but the README/INI template should point `Theme` at users who want the mode to follow.
- [Appearance regression in light mode] → `toolbarBg #F0F0F0` approximates the current `COLOR_BTNFACE`; users on unusual OS themes lose that subtle custom tint. Acceptable within "fully theme-owned" (explicit decision).
- [Qt checked-state rendering differs from Win32 owner-draw] → Semantic parity (checked emphasis from the palette) rather than pixel parity; documented in the spec scenario wording.
- [High-contrast/accessibility OS settings ignored by forced colors] → Pre-existing behavior (page/sidebar already forced); unchanged by this design.

## Migration Plan

- No config migration: `BackgroundColor`/`SidebarBackground` keep working identically in light mode; `Theme` is additive.
- Rollback: revert the change; behavior returns to today's light (system-driven) chrome.

## Open Questions

None — specs, approach, and task breakdown are settled.