## Context

See proposal.md - Why. The viewer currently has two display modes (paged/continuous) plus three fit modes (manual/fit-to-page/fit-to-width). All geometry flows through `ViewerController::computeLayout()` (`m_pageRects` per page + `m_contentSize`) and `computeFitZoom()`; the two viewers (`viewer_win32.*` / `viewer.*`) only differ in how they **paint** and **scroll** a layout — paged mode centers a single current page, continuous mode walks the strip rects. The `toolbar.*` presenter/backend pair already maps a shared control set onto both native toolbars.

The two-page feature layers a new orthogonal axis (one-page vs two-page presentation) on top of the existing paged/continuous and fit axes. The existing `m_pageRects`/`m_contentSize` machinery and the viewer paint/scroll split are the right seams: that machinery already handles mixed page sizes and per-page rendering, so two-page needs only a different layout and paint/scroll branches, not new cache or render infrastructure.

## Goals / Non-Goals

**Goals:**
- Add a presentation toggle (one-page/two-page) independent of the paged/continuous mode and fit mode.
- In two-page, display two adjacent pages as one unit for paged display, continuous scrolling, and all fit modes.
- Keep one-page behavior byte-for-byte identical to today.
- Shared logic lives in `ViewerController`; viewers only add two-page-specific paint/scroll branches (matching the existing platform split).
- Add a toolbar button between the display-mode toggle and the fit button, wired through the shared presenter on both platforms.

**Non-Goals:**
- No new rendering, caching, or engine changes — `renderPageCached` per page stays intact.
- No per-parity "book" semantics such as treating page 1 alone as a cover; pairing is simply consecutive (1,2),(3,4),... and an odd trailing page is shown alone.
- No changes to text selection, search, print, outline sidebar, or rotation behavior beyond what their shared layout math already picks up.
- No configurable/memorized default; the presentation starts at one-page on every open.

## Decisions

### D1: Presentation flag lives in `ViewerState`
Add `bool m_twoPage = false;` with `setTwoPage(bool)` / `isTwoPage() const` to the existing value-type `ViewerState`, mirroring `m_pagedMode`. Rationale: it is plain state that must survive across layout rebuilds and resets on open/close, exactly like the display mode. Default `false` gives the required one-page default and `closeDocument()` reset for free.

### D2: Pairing and navigation anchor on the left page of each unit
Pages are 1-based. Two-page units are (1,2), (3,4), (5,6), … For a unit, the **left** page is `unitBase = (page - 1)` rounded down to the next pair boundary, i.e. `((page-1)/2)*2 + 1`; the right page is `unitBase+1` when `unitBase+1 <= pageCount`, else the left page is shown alone. `m_currentPage` always points at the unit's left page in two-page mode.

Helpers on `ViewerController`:
- `pageCount()` unchanged (physical page count).
- `int unitPage(int leftPage) const` → returns `leftPage + (leftPage < pageCount ? 1 : 0)` (the partner, or the page itself when unpaired).
- Navigation `nextPage()`/`prevPage()` in two-page mode step by **2** page indices, clamped to a valid unit; `goToPage(n)` resolves to `unitBase(n)`. This makes wheel, keyboard, and page-box all march unit to unit.

Rationale: keeping `m_currentPage` as a left page means every "current page" maps to exactly one unit and all existing anchors (`scrollOffsetForPage`, page tracking, sidebar highlight) keep working without a second index. Alternative — a separate "unit index" state — was rejected as it would duplicate tracking and complicate every consumer.

### D3: Layout builds two-page units in `computeLayout()`
When `isTwoPage()`, `computeLayout()` lays pages in horizontal pairs. Each page still gets its own entry in `m_pageRects` (so render/cache/text-selection stay per-page and unchanged), but pair members share the same vertical cursor:

```
for each unit (pairs of pages):
    left = page i, right = page i+1 (or none)
    rowH = max(height(left), height(right or 0))
    place left at (x_left, cursor), right at (x_right, cursor)
    canvas width = max single-page width seen so far OR combined unit width (see D4)
    cursor += rowH + kPageGap
```

The canvas is at least the viewport wide, and the pair is centered horizontally, consistent with the current single-page centering. When a page is unpaired, it is centered on its row.

`firstPageAtScroll`/`pageAtScrollOffset` continue to work off `m_pageRects`; because pair pages share a vertical stride, both members enter/leave the viewport together, yielding the required "scroll both as one".

### D4: Fit modes target the whole unit in `computeFitZoom()`
In two-page mode `computeFitZoom()` measures the current unit, not a single page:
- combined width = `leftW + kPageGap + rightW` (right = 0 when unpaired)
- unit height = `max(leftH, rightH)` (right = 0 when unpaired)
- **fit-to-width** zoom = `viewportW / combinedWidth`
- **fit-to-page** zoom = `min(viewportW / combinedWidth, viewportH / unitHeight)`

This satisfies "fit both pages by width" and "fit both pages to the screen". Manual zoom and the 0.1–5.0 clamp are unchanged.

### D5: Viewer paint/scroll branches are presentation-aware
Both viewers already branch on `isPagedMode()`. Add a second branch on `isTwoPage()` that composes the unit from the two cached page bitmaps:
- **Win32 `onPaint`**: in the paged branch, when two-page, blit both pages of the unit side by side (centered together when the unit fits; panned together when it overflows). In the continuous branch the existing strip loop already walks `m_pageRects`, so with D3's side-by-side rects it paints a two-page row at no extra cost.
- **Qt `ViewerCanvas::paintEvent`**: same — paged branch draws both pages of the unit; continuous branch walks `m_pageRects` unchanged. Selection/search overlays are drawn per-page against `m_pageRects`, which still per-page rects, so they need no change beyond what the layout supplies.

Only the paged branch needs real new drawing (continuous is emergent from D3). Rotation is unchanged and already applied inside `computeLayout`'s dimension swap.

### D6: Shared presenter command + per-platform toolbar wiring
Add a `toolbar::Control::PresentationToggle` between `ModeToggle` and `FitButton` in `toolbar.h`, a `toolbar::Icon` for it, and `ToolbarPresenter::onPresentationToggled()` that calls `controller->toggleTwoPage()` then refreshes state. `refreshState()` sets the button's checked state from `controller->isTwoPage()`.

Both backends add the control:
- **Win32** (`toolbar_win32.*`): a new checkable owner-drawn BUTTON with a new `ID_*`, placed after `ID_MODE`, wired to `onPresentationToggled()` in `onCommand()`.
- **Qt** (`toolbar_qt.*`): a new `QToolButton` with lambda `onPresentationToggled()`, inserted after the mode toggle.

### D7: Keyboard shortcut for the presentation toggle
Add a viewer keyboard command that toggles two-page presentation, equivalent to the toolbar button, so keyboard and toolbar cannot diverge (the state-changed callback keeps the button in sync). Concrete key choice is left to the implementer/UI, but it must be a key not already bound (V = mode, Shift+V = fit, R = rotate, +/-/0 = zoom, arrows/PgUp/PgDn/Home/End = navigation, G = go-to, Esc, Ctrl+C).

### D8: Fit-zoom target is the current unit, kept consistent with D3
`computeFitZoom()` and `computeLayout()` must both derive the unit from the same pairing rule (D2/D3) so fit and layout never disagree on which pages are adjacent. Implement by sharing one small `unitBounds(leftPage)` helper used by both.

## Platform-specific code

| Concern | Win32 (`viewer_win32.*`, `toolbar_win32.*`) | Qt (`viewer.*`, `toolbar_qt.*`) |
|---|---|---|
| Presentation flag + layout + fit + navigation | `ViewerController` (shared, unchanged per platform) | same |
| Paged two-page paint | `onPaint` paged branch blits both page HBITMAPs side by side; clamp/pan offsets follow the existing overflow path | `paintEvent` paged branch draws both page QImages; overlay code unchanged |
| Continuous two-page paint | emergent from D3 rects; existing strip loop | emergent from D3 rects; existing loop |
| Keyboard toggle | new case in `onKeyDown` (focus-neutrality guard already applies) | new `QShortcut` + handler |
| Toolbar control | new `ID_*` button in owner-drawn strip, placed after `ID_MODE` | new `QToolButton` after mode toggle |
| Scroll anchoring | uses existing `applyScroll`/`scrollAnchor` | uses existing scrollbar-set path |

## Risks / Trade-offs

- **Odd page count leaves a lone final page** → By design (spec: "Odd trailing page shown alone"); it renders centered on its row, matching single-page behavior. Verified by the `unitPage` helper returning the page itself when unpaired.
- **Two-page in continuous yields a much wider canvas than the viewport** → Only the horizontal extent within the pair is wider than one page; the canvas width is bounded by `max(combined unit width, viewport)` via D3, and horizontal scrollbar behavior (`maxScrollOffsetX`) already supports overflow. Mitigation: keep `m_contentSize` width = max unit width so fit-by-width and centering agree.
- **Fit-to-page trade-off in two-page** → Fitting two pages can make each page small on narrow viewports. This is the documented behavior ("fit both pages to the screen"); fit-to-width and manual zoom remain available to trade off.
- **Navigation stepping by 2 may skip odd trailing page on advance** → `nextPage` clamps to the last valid unit, so an unpaired last page is never skipped over and remains reachable from its own unit.
- **Regression risk to one-page behavior** → The two-page flag defaults off and every shared path keeps its existing one-page branch; the change is strictly additive. Mitigation: verify one-page renders/scrolls identically after the change (see tasks).
- **Overlay (selection/search) alignment in two-page** → Overlays key off per-page `m_pageRects`, which stay correct; only the paged two-page drawing path must place pages at the same coordinates the overlays expect.

## Migration Plan

No deployment/rollback story beyond a normal build: this is an additive feature in a single plugin binary. Rollback is reverting the change and rebuilding the plugin. No data or dependency migration applies.

## Open Questions

None — grouping, interaction with all three fit modes, odd-page handling, and the toolbar placement are all specified. The specific keyboard key to bind (D7) is a pure key-mapping choice and can be settled during implementation without affecting specs, approach, or tasks.
