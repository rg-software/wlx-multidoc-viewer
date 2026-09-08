## Context

See `proposal.md` — Why/What. The plugin already reads INI settings from `[Viewer]` through a lazy, cached config singleton (`PluginConfig::get()`, `mINI`) and exposes them as `inline` constants in `src/viewer_settings.h` (`kBackgroundColor` via `parseHexColor`). The outline sidebar currently paints with platform defaults — `COLOR_WINDOW` / `COLOR_BTNFACE` class brushes on Win32 (`sidebar_win32.cpp:31,46`), the default Qt palette on Linux (`sidebar_qt.cpp`, no explicit background anywhere). The change adds a `[Viewer] SidebarBackground` key that colors the sidebar on both platforms, reusing the existing hex-color parsing. Requirements live in `openspec/specs/plugin-config/spec.md`; this change adds a delta requirement.

## Goals / Non-Goals

**Goals:**
- Add `kSidebarBackground` in `viewer_settings.h`, parsed from `[Viewer] SidebarBackground` with the same `parseHexColor` rules as `BackgroundColor`, falling back to `0xE8E8E8`.
- Make the Win32 sidebar (panel class regions backed by the panel brush, the TreeView background, and the grip) render with the configured color.
- Make the Qt sidebar (container widget, tree viewport, grip) render with the configured color.
- Document the new key in `assets/multidocviewer.ini`.

**Non-Goals:**
- Theming the sidebar text color (tree item foreground / selected state) — background only.
- A `[Sidebar]` INI section — the two existing sidebar keys (`SidebarWidth`, `SidebarVisible`) already live under `[Viewer]`; the new key follows that precedent.
- Making `SidebarBackground` default to the page-area `BackgroundColor` (see D4).
- Sidebar background for the toolbar, grip hover/active visuals, or host-provided chrome.

## Decisions

**D1 — A standalone `kSidebarBackground` constant, parsed independently of `kBackgroundColor`.**
`viewer_settings.h` gains `inline uint32_t kSidebarBackground = parseHexColor(PluginConfig::get().get("Viewer").get("SidebarBackground"), 0xE8E8E8);`, mirroring `kBackgroundColor`. Both platforms read the same constant; the existing per-platform conversion helpers (`QRgb`-to-`COLORREF` on Win32, `QColor` on Qt) are reused at the paint sites.
*Alternatives rejected:* reusing `kBackgroundColor` directly (couples the two settings, contradicts the spec's independence scenarios); threading the color through constructors (unnecessary plumbing — the constant is already global).

**D2 — On Win32, color both the panel brush and the TreeView control.**
The `WLX_SIDEBAR_CLASS` class brush (`COLOR_WINDOW + 1`) is replaced with a brush built from `kSidebarBackground`, and `TVM_SETBKCOLOR` (via `TreeView_SetBkColor`) is sent to `m_tree` with the same value so the tree viewport — which dominates the sidebar's visible area — matches. The grip class brush (`COLOR_BTNFACE + 1`) is likewise replaced so the resize strip blends with the panel.
Brush lifetime: the window classes are registered once (`static bool registered`) and outlive any `SidebarWin32` instance, so the brushes must also be process-lifetime. A function-local `static HBRUSH` created once (never `DeleteObject`-ed) is stored in both `WNDCLASSEXW.hbrBackground` members; on process teardown the OS reclaims it. This matches the existing "class registered once" pattern and avoids a use-after-free if a second viewer instance is created after the first was destroyed.
*Alternatives rejected:* per-instance member brushes (would outlive-instance via the static-registered class → dangling brush on a second viewer); `WM_ERASEBKGND` painting (more code, must also handle the tree's own background).

**D3 — On Qt, set a `QPalette` on the sidebar container and the tree.**
Adopt the pattern already used for the scroll-area viewport (`viewer.cpp:265-277`): build `QColor` from `kSidebarBackground`, set `QPalette::Window` + `QPalette::Base` on `m_tree` and `m_tree->viewport()` (the viewport has its own palette copy at construction), set `QPalette::Window` on the container `this`, and enable `setAutoFillBackground(true)` on all three. The grip (`ResizeGrip`) is a plain `QWidget`; setting the container's palette covers it via palette inheritance, and setting `Base` on the tree covers tree rows.
*Alternatives rejected:* stylesheets (`m_tree->setStyleSheet(...)`) — heavier, Qt-theme-dependent, and inconsistent with the palette-based approach used elsewhere.

**D4 — Independent default `0xE8E8E8` when the key is absent or malformed.**
Same fallback as the page-area default, so a stock setup looks uniform. Deliberately **not** derived from `BackgroundColor`: deriving couples two independent keys and changes behavior for users who only set `BackgroundColor`. Today's sidebar defaults already differ per platform (Win32 white-ish `COLOR_WINDOW`, Qt theme) — an explicit light-gray default additionally unifies them.
*Alternatives rejected:* fall back to `kBackgroundColor` (coupling); keep platform system colors on absent key (requires a sentinel value for `parseHexColor` and platform-specific behavior, and preserves the current inconsistency).

**D5 — Template documentation in `assets/multidocviewer.ini`.**
Add to `[Viewer]`:
```ini
; Outline sidebar background color as a hex RGB value
SidebarBackground = #E8E8E8
```
The key is read once at startup and cached like all other settings; editing it requires a plugin reload.

## Platform-Specific Code

- **Windows (`src/viewer_settings.h`, `src/sidebar_win32.cpp`):** `kSidebarBackground` parsed by the shared `parseHexColor`; a process-lifetime `HBRUSH` from `RGB(r,g,b)` assigned to both window-class brushes; `TreeView_SetBkColor` on the tree. No changes to `viewer_win32.cpp` (page-area background is already handled by `BackgroundColor`).
- **Linux (`src/viewer_settings.h`, `src/sidebar_qt.cpp`):** same shared constant; `QPalette` + `setAutoFillBackground` applied to the container, `m_tree`, and `m_tree->viewport()`. No changes to `viewer.cpp`.
- **Shared (`src/viewer_settings.h`, `assets/multidocviewer.ini`):** the constant definition, its hex-parse fallback, and the template documentation. The parse rule (case-insensitive, optional `#`, 6 hex digits) is reused verbatim from `parseHexColor`.

## Risks / Trade-offs

- [Dark sidebar background leaves theme-colored tree text hard to read on Win32] → Background-only scope; the setting is documented as background color, and users choosing a dark sidebar accept the default text color. Adjusting text/selection colors is a separate change.
- [Win32 `TVM_SETBKCOLOR` does not recolor the `WS_EX_CLIENTEDGE` border or expansion arrows] → Cosmetic; the border remains theme-drawn. Accepted mismatch, invisible on most themes.
- [Brush lifetime tied to static-registered window class] → Process-lifetime static `HBRUSH`, never freed; recreated from the cached setting (already stable after first parse), so no ownership hazards across viewer instances.
- [Changing the sidebar default from `COLOR_WINDOW`/theme to a fixed light gray alters today's look] → Intentional and consistent across platforms; users who want the old look can set `SidebarBackground` explicitly or leave the uniform default.

## Migration Plan

No active migration — the INI is read-only, cache-loaded, and defaults are compile-time. Rollback for a user is removing the key (or setting it to the previous system look). The template file ships the new key commented as an example value; existing installed templates are unchanged until the plugin is re-deployed.