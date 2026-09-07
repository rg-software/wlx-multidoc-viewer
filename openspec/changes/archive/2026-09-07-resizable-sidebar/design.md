## Context

See `proposal.md` — Why/What. Current implementation facts that shape the approach:

- The sidebar's initial state and width are hard-coded: `viewer_settings::kSidebarBaseWidth = 180` logical px and "starts hidden" (`m_sidebarVisible = false` on load). INI config already flows through `PluginConfig::get()` into `viewer_settings` (the `kBackgroundColor` pattern), so the sidebar's default visibility and width reuse that same read-only path.
- `SidebarWin32` derives its on-screen width from logical base × DPI: `widthPx()` returns `kSidebarBaseWidth * m_dpiScale` (device px). `SidebarQt` calls `setFixedWidth(kSidebarBaseWidth)` directly (Qt is inherently logical-px). `SidebarQt::setWidth(int)` exists but is never called.
- Win32 chrome chain: `ViewerWin32::sidebarLeft()` → `layoutChrome()` (`MoveWindow` the child) → `setLeftChrome()` → `controller->relayout()` → `onControllerChanged()`. `showHideSidebar()` already performs this whole sequence, so a width change can reuse it verbatim.
- Qt chrome: sidebar + scroll area share `m_midLayout` (QHBoxLayout); `onSidebarToggle()` already calls `m_controller->setLeftChrome(m_sidebar->width())`; a fixed-width change reflows the layout automatically.
- Entry reveal happens in exactly two places per backend: `selectEntry(id)` (navigation / page-driven highlight, guarded by `m_lastSelected`) and `clearEntries()` (reload).
- The shared `SidebarBackend` interface (sidebar.h) is the single cross-platform contract; the toolbar uses the same `std::function`-callback pattern (`setScrollApplier`) for cross-boundary events.

## Goals / Non-Goals

**Goals:**
- Drag-to-resize of the sidebar on both platforms, session-scoped, defaulting to the ini initial width (180 logical px) per new viewer window.
- Bounded widths: min 80 logical px; max half the lister page-area width (live-relayout during the drag).
- Ini-fed defaults: `[Viewer] SidebarVisible` decides whether an outlined document shows the sidebar on load; `[Viewer] SidebarWidth` sets the starting width of a new viewer window. Both read-only.
- Tree always reveals the *beginnings* of lines: horizontal scroll resets to the left edge on entry reveal and on reload; manual horizontal scrolling stays available afterward.
- One small shared interface addition; identical UX; no persistence.

**Non-Goals:**
- Persisting the session-resized width to disk — the ini is read-only (defaults only), never written by the plugin; the user's drag-adjusted width is never saved.
- Content-derived ("preferred") initial width — the tree populates lazily, so only top-level entries exist at load time and any measured width would be partial, content-dependent, and platform-divergent. `SidebarWidth` is the sole source of the initial width.
- Keyboard resize affordance, toolbar indicator, or a left-edge grab handle.
- Anything that would lock or disable horizontal scrolling entirely.

## Decisions

**D1 — Width is stored as a logical base value, scaled to device px only at paint/layout.**
`SidebarWin32` gains `m_baseWidthPx` (logical) and `widthPx()` becomes `m_baseWidthPx * m_dpiScale`; the grip converts drag deltas with the inverse (`logical = device / m_dpiScale`). `SidebarQt` already operates in logical px (fixed width). Storing logical keeps one source of truth for the session value and survives `WM_DPICHANGED` / DPI-slider moves without re-clamping.
*Alternatives rejected:* store device px in the backend — Qt has no stable device concept, and Win32 would need a rescale on every DPI change.

**D2 — Resize grip is a dedicated child strip on the sidebar's right edge.**
Both platforms add a constant-width grip (`kSidebarGripWidthPx = 6`) taking the right edge of the sidebar panel; the tree is laid out to fill `sidebarWidth - gripWidth`.
- Win32: a child `HWND` (consistent with `toolbar_win32`'s child-hwnd approach) handling `WM_SETCURSOR` (IDC_SIZEWE), `WM_LBUTTONDOWN` (SetCapture), `WM_MOUSEMOVE` (compute candidate from `GetCursorPos` minus panel origin), `WM_LBUTTONUP`/`WM_CAPTURECHANGED` (ReleaseCapture → done).
- Qt: a `QWidget` with `SizeHorCursor`, `mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent`.
*Alternatives rejected:* hit-testing a right-edge strip inside the subclassed tree — couples drag logic into the tree's already-subclassed wndProc/eventFilter and fights the tree's own hit-testing.

**D3 — One shared callback on `SidebarBackend` carries width changes to the viewer.**
`SidebarBackend::setWidthChangedHandler(std::function<void(int logicalPx)>)`, mirroring `SidebarPresenter::setScrollApplier`. The backend computes the candidate width from drag deltas; the viewer is responsible for clamping and re-running the chrome chain. Drag math, min-clamp, and capture live in the backend; the max-clamp (which depends on the viewer's client size) and relayout live in the viewer.
*Alternatives rejected:* platform-specific wiring duplicated per viewer — they would drift; a backend that also clamps max would need the client size injected into it.

**D4 — Max bound = half the page area, clamped from below by the min.**
When the sidebar is visible the page area shrinks by the sidebar width, so the requirement resolves to `sidebar ≤ (clientW − sidebar) / 2`, i.e. `sidebar ≤ clientW / 3`. The viewer computes `maxAllowed = max(kSidebarMinWidth, clientLogicalW / 3)` and clamps every drag candidate (Win32 converts to device px before `MoveWindow`; Qt clamps in logical px before `setWidth`). A tiny window degrades to the min width rather than a broken state.

**D5 — Horizontal reveal is a single "scroll to left" on the two existing reveal points.**
- Win32: `SendMessage(m_tree, WM_HSCROLL, SB_LEFT, 0)` immediately after `TreeView_EnsureVisible` in `selectEntry`, and at the end of `clearEntries`. Both run inside the existing `WM_SETREDRAW FALSE/TRUE` guard (Extend the guard to cover the reset, then invalidate once).
- Qt: `m_tree->horizontalScrollBar()->setValue(0)` after `scrollToItem` in `selectEntry` and at the end of `clearEntries`.
This covers the spec scenarios: reveal → left (navigation), reload → left (`clearEntries`), manual scroll → untouched (no reset outside those two points).
*Alternatives rejected:* relying on `EnsureVisible`'s default scroll bias (an item already visible on-screen can leave the viewport scrolled right of the start), and disabling the horizontal scrollbar entirely (breaks "user may still scroll", and would clip long titles forever).

**D6 — Live relayout reuses the existing resize path.**
Each drag-step width change runs the same body as `showHideSidebar`: Win32 `layoutChrome()` + `setLeftChrome(sidebarLeft())` + `relayout` + `onControllerChanged()`; Qt `setWidth` (auto reflow via `m_midLayout`) + `setLeftChrome` + `relayout` + `onControllerChanged()`. This is the identical code path already executed on every window resize, so no new rendering mechanics are introduced (the per-page render cache bounds the cost). Qt drag reentrancy (handler → setWidth → layout → events) is guarded with a `m_resizing` flag.

**D7 — Constants and ini-fed defaults live in `viewer_settings.h`.**
Keep `kSidebarBaseWidth = 180` as the fallback. Add `kSidebarInitialWidth` (parsed from `[Viewer] SidebarWidth`, clamped to ≥ `kSidebarMinWidth`, defaults to 180) and `kSidebarVisibleByDefault` (parsed from `[Viewer] SidebarVisible`, defaults to false, i.e. start hidden) — both using the same `PluginConfig::get()` read as `kBackgroundColor`. Add `kSidebarMinWidth = 80` and `kSidebarGripWidthPx = 6`. Both viewers and both backends reference the shared values, so the spec'd 80/180/6 remain in one place.

**D8 — An oversized ini width is clamped at first layout, and no width is content-derived.**
`kSidebarInitialWidth` is clamped from below by `kSidebarMinWidth`; a value exceeding half the page area is clamped by the viewer's existing max rule (`clientLogicalW / 3`) the first time the sidebar is laid out — same code path as a drag candidate. A "preferred width" derived from title text is rejected: lazy tree population means only top-level entries exist at load, so the measurement would be partial and inconsistent across platforms; `[Viewer] SidebarWidth` is the sole, deterministic source.

## Risks / Trade-offs

- [Drag-time relayout cost on huge documents] → The relayout chain is the same one used on every window resize and is bounded by the per-page render cache (`kCacheWindowPages`); nothing scales with page count.
- [Horizontal reset flicker on long selected entries] → Perform the reset inside the `WM_SETREDRAW` guard (Win32); a single `InvalidateRect`/Qt reflow after. The `m_lastSelected` guard already prevents redundant redraws for same-entry page changes.
- [Grip overlapping the tree or stealing boundary clicks] → Tree is sized to `panel − grip`, so the tree's client area never extends under the grip; the grip is `WS_EX_NOPARENTNOTIFY`, non-focusable (`WS_EX_NOACTIVATE` on Win32), and click-through is irrelevant at 6 px.
- [Qt feedback loop between `setFixedWidth` and drag mouse-move] → `m_resizing` reentrancy guard; release stops the drag cleanly even on capture loss.
- [Max-clamp formula edge cases (very small lister)] → Clamped from below by `kSidebarMinWidth`, so the sidebar can never collapse to a sliver.

## Migration Plan

- No persisted data or config to migrate; a rollback is a straight revert of the change.
- Ship order: shared constants + `SidebarBackend` callback first, then Win32 backend/viewer, then Qt backend/viewer; build gate both presets before marking the task complete.
- Win32 smoke: resize drag clamps at min and ½-page-area, DPI change keeps the logical width, reload keeps width, new window resets to 180. Qt: same plus live layout reflow.

## Platform-Specific Code

- **Win32 (`sidebar_win32.*`, `viewer_win32.cpp`):** grip child `HWND` + `WM_SETCURSOR`/capture/lbuttondown/mousemove/mouseup; logical↔device conversion via `m_dpiScale` before calling the shared handler; `setBaseWidth(int logicalPx)`; `widthPx()` initialized from `kSidebarInitialWidth`; default visibility applied on document load via `showHideSidebar(kSidebarVisibleByDefault)` instead of the hard-coded hidden start; viewer clamps max (device px), then reuses the `showHideSidebar` tail (`layoutChrome` → `setLeftChrome` → `relayout` → `onControllerChanged`); `SendMessage(m_tree, WM_HSCROLL, SB_LEFT, 0)` inside the redraw guard in `selectEntry`/`clearEntries`.
- **Qt (`sidebar_qt.*`, `viewer.cpp`):** grip `QWidget` with mouse handlers + `SizeHorCursor`; `setWidth` (already exists) applies logical px; initial width from `kSidebarInitialWidth`; default visibility from `kSidebarVisibleByDefault` on load; viewer clamps max and calls `setLeftChrome` + relayout; `m_tree->horizontalScrollBar()->setValue(0)` in `selectEntry`/`clearEntries`.