## Context

See proposal.md - Why. The viewer currently has two display modes (paged/continuous) plus three fit modes (manual/fit-to-page/fit-to-width). All geometry flows through `ViewerController::computeLayout()` (`m_pageRects` per page + `m_contentSize`) and `computeFitZoom()`; the two viewers (`viewer_win32.*` / `viewer.*`) only differ in how they **paint** and **scroll** a layout — paged mode centers a single current page, continuous mode walks the strip rects. The `toolbar.*` presenter/backend pair already maps a shared control set onto both native toolbars.

The double-page feature layers a new orthogonal axis (a three-state page-presentation: single-page, double-page, double-page-with-cover) on top of the existing paged/continuous and fit axes. The existing `m_pageRects`/`m_contentSize` machinery and the viewer paint/scroll split are the right seams: that machinery already handles mixed page sizes and per-page rendering, so double-page needs only a different layout and paint/scroll branches, not new cache or render infrastructure.

## Goals / Non-Goals

**Goals:**
- Add a three-state presentation control (single-page / double-page / double-page-with-cover) independent of the paged/continuous mode and fit mode.
- In a double-page state, display the unit's pages side by side for paged display, continuous scrolling, and all fit modes.
- `double-page-with-cover` gives page 1 its own dedicated area and pairs the remainder (2,3),(4,5),...; `double-page` pairs from the start (1,2),(3,4),...
- Keep single-page behavior byte-for-byte identical to today.
- Shared logic lives in `ViewerController`; viewers only add double-page-specific paint/scroll branches (matching the existing platform split).
- Add a three-state toolbar control between the display-mode toggle and the fit button, wired through the shared presenter on both platforms.

**Non-Goals:**
- No new rendering, caching, or engine changes — `renderPageCached` per page stays intact.
- No changes to text selection, search, print, outline sidebar, or rotation behavior beyond what their shared layout math already picks up.
- No configurable/memorized default; the presentation starts at single-page on every open.
- No detection or styling of an actual cover image; "cover" here means only that page 1 is placed in its own dedicated row.

## Decisions

### D1: Presentation state lives in `ViewerState` as a three-state enum
Add a `PagePresentation` enum with three values — `Single`, `Double`, `DoubleWithCover` — and store it in an `int m_pagePresentation` (or the enum member) defaulting to `Single`, plus `setPagePresentation(PagePresentation)` / `pagePresentation() const`, mirroring `m_pagedMode`. Rationale: what was previously a boolean one-page/two-page flag is now a three-way choice, so an enum read/written as a whole is simpler than two booleans. Default `Single` gives the required single-page default and `closeDocument()` reset for free.

### D2: Pairing and navigation anchor on the first (leftmost) page of each unit
A **unit** is the set of pages displayed together. Pages are 1-based. The unit containing page `p` and its members depend on the presentation:

- **Single**: unit = {p}.
- **Double**: units are (1,2), (3,4), (5,6), … The first page of the unit containing `p` is `unitFirst = ((p-1)/2)*2 + 1`; members are `{unitFirst, unitFirst+1}` (the second only when ≤ `pageCount`).
- **DoubleWithCover**: page 1 forms a singleton unit; the remaining pages pair as (2,3), (4,5), (6,7), …. The first page of the unit containing `p` is `1` when `p == 1`, else `( (p-2)/2 )*2 + 2`.

`m_currentPage` always points at the first (leftmost / only) page of the active unit.

Helpers on `ViewerController`:
- `pageCount()` unchanged (physical page count).
- `int unitFirst(int page) const` → first page of the unit containing `page` (above rule).
- `int unitLast(int unitFirst) const` → the unit's second member, or `unitFirst` itself when the unit is a singleton (cover page 1 or an unpaired trailing page).
- Navigation `nextPage()`/`prevPage()` in a double-page state step by **2** page indices (except from a singleton unit, which advances by the unit stride too), clamped to a valid unit; `goToPage(n)` resolves to `unitFirst(n)`. This makes wheel, keyboard, and page-box all march unit to unit.

Rationale: keeping `m_currentPage` as a unit's first page means every "current page" maps to exactly one unit and all existing anchors (`scrollOffsetForPage`, page tracking, sidebar highlight) keep working without a second index. The single pairing rule is shared by layout, fit, and navigation so they never disagree. Alternative — a separate "unit index" state — was rejected as it would duplicate tracking and complicate every consumer.

The page counter keeps **real page numbers**, not screen indices: the toolbar `PageBox` shows the current unit's first page and `PageCount` the physical page count (e.g. `3 / 6` for unit (3,4) in double page), leaving the go-to dialog, sidebar outline, and search results on real pages. Next/previous controls are enabled via `hasNextPage()`/`hasPrevPage()`, which test for an adjacent *unit* (`unitLast(currentPage) < pageCount`), not a raw page boundary — so on the final unit (whose first page is below the count) Next is disabled instead of silently no-oping. In continuous mode the controls use `scrollStepPage(base, ±1)`: from the page at the scroll anchor it returns the neighbouring unit's first page in a double-page state (whole rows move together, with the cover page as its own singleton unit) or the raw `base ± 1` in single-page — matching the keyboard stepping while keeping toolbar and keyboard aligned. The step reports a no-op (target == base) past the first/last unit, which both gate the button enablement and make the press a no-op in the same situation. Because the toolbar base derives from `scrollAnchor()`, the Win32 viewer re-syncs that anchor (and re-refreshes the toolbar) at the end of `applyScroll` — the notify fired during `goToPage` would otherwise leave the enablement showing the pre-jump position (typically page 1: Prev disabled, Next enabled) until the next scroll event — and the key-down continuous paths (arrows/Home/End/P/V/PgDn/PgUp) call `setScrollAnchor(m_scrollY)` before painting; the Qt scrollbar path already syncs in `onVerticalScrollChanged`.

### D3: Layout builds units in `computeLayout()`
When the presentation is a double-page state, `computeLayout()` lays units in horizontal pairs (a singleton unit occupies just its own page). Each page still gets its own entry in `m_pageRects` (so render/cache/text-selection stay per-page and unchanged), but unit members share the same vertical cursor:

```
for each unit (sorted by unitFirst):
    first = unitFirst, last = unitLast(first)
    rowH = max(height(first), height(last != first ? last : 0))
    place first alone when singleton, else place first at (x_left, cursor) and last at (x_right, cursor)
    canvas width = max single-page width seen so far OR combined unit width (see D4)
    cursor += rowH + kPageGap
```

The canvas is at least the viewport wide, and each unit is centered horizontally, consistent with the current single-page centering. Singleton units (cover page 1, unpaired last page) are centered on their row.

`firstPageAtScroll`/`pageAtScrollOffset` continue to work off `m_pageRects`; because unit members share a vertical stride, they enter/leave the viewport together, yielding the required "scroll as one".

### D4: Fit modes target the whole unit in `computeFitZoom()`
In a double-page state `computeFitZoom()` measures a unit, not a single page:
- combined width = `firstW + (last != first ? kPageGap + lastW : 0)`
- unit height = `max(firstH, last != first ? lastH : 0)`
- **fit-to-page** zoom = `min(viewportW / combinedWidth, viewportH / unitHeight)` (targets the current unit)
- **fit-to-width** zoom = `viewportW / maxRowWidth()`, where `maxRowWidth()` is the widest combined unit across the document (a single page in a single-page state)

This satisfies "fit the unit by width" and "fit the unit to the screen". Fit-to-width deliberately targets the document-wide spread, not the current unit: when the current unit is a singleton (cover page, trailing odd page) zoom still fits a two-page spread, so the next spread fits the viewport width without overflow and does not depend on which page the fit was triggered from. For a singleton unit the fit-to-page formula reduces to the existing single-page fit. Manual zoom and the 0.1–5.0 clamp are unchanged.

### D5: Viewer paint/scroll branches are presentation-aware
Both viewers already branch on `isPagedMode()`. Add a second branch on the double-page presentation that composes the unit from the cached page bitmaps:
- **Win32 `onPaint`**: in the paged branch, when double-page, blit the unit's pages side by side (centered together when the unit fits; panned together when it overflows). In the continuous branch the existing strip loop already walks `m_pageRects`, so with D3's side-by-side rects it paints a unit row at no extra cost.
- **Qt `ViewerCanvas::paintEvent`**: same — paged branch draws the unit's pages; continuous branch walks `m_pageRects` unchanged. Selection/search overlays are drawn per-page against `m_pageRects`, which are still per-page rects, so they need no change beyond what the layout supplies.

Only the paged branch needs real new drawing (continuous is emergent from D3). Rotation is unchanged and already applied inside `computeLayout`'s dimension swap.

### D6: Shared presenter command + per-platform toolbar wiring
Add a `toolbar::Control::PresentationToggle` between `ModeToggle` and `FitButton` in `toolbar.h`, `toolbar::Icon` glyphs for the states, and `ToolbarPresenter::onPresentationCycled()` that calls `controller->cyclePagePresentation()` then refreshes state. `refreshState()` sets the control's current state from `controller->pagePresentation()`, and the control acts as a **three-state cycle**: each activation moves single → double → double-with-cover → single.

Both backends add the control:
- **Win32** (`toolbar_win32.*`): a new owner-drawn BUTTON with a new `ID_*`, placed after `ID_MODE`, that cycles through the three states on each click, wired to `onPresentationCycled()` in `onCommand()`.
- **Qt** (`toolbar_qt.*`): a new `QToolButton` with lambda `onPresentationCycled()`, inserted after the mode toggle, cycling the three states.

The display-mode, fit and presentation glyphs are drawn from re-carved Material Symbols codepoints (`toolbar_icons.cpp`): the embedded `assets/MaterialSymbolsOutlined.ttf` is a ~4.3 KB default-instance carve (24 codepoints) of the Material Symbols Outlined variable font at FILL 0/GRAD 0/opsz 24/wght 400, and the previously-drawn icons now render real glyphs — paged → `check_box_outline_blank` 0xe835, fit manual → `pinch` 0xeb38, fit page → `fit_page` 0xf77a, fit width → `fit_width` 0xf779, presentation single → `article` 0xef42, presentation double → `two_pager` 0xf51f, presentation double-with-cover → `chrome_reader_mode` 0xe86d. The `hasInk()` guard still falls back to the programmatic vector shapes if FreeType ever rasterizes empty. `ModeContinuous` stays the font `view_agenda` glyph.

### D7: Keyboard shortcut for the presentation control
Add a viewer keyboard command that cycles the page-presentation state, equivalent to the toolbar control, so keyboard and toolbar cannot diverge (the state-changed callback keeps the control in sync). Concrete key choice is left to the implementer/UI, but it must be a key not already bound (V = mode, Shift+V = fit, R = rotate, +/-/0 = zoom, arrows/PgUp/PgDn/Home/End = navigation, G = go-to, Esc, Ctrl+C).

### D8: Fit-zoom target is the current unit, kept consistent with D3
`computeFitZoom()` and `computeLayout()` must both derive the unit from the same pairing rule (D2/D3) so fit and layout never disagree on which pages are adjacent. Implement by sharing one small `unitBounds(unitFirst)` helper used by both.

## Platform-specific code

| Concern | Win32 (`viewer_win32.*`, `toolbar_win32.*`) | Qt (`viewer.*`, `toolbar_qt.*`) |
|---|---|---|
| Presentation state + layout + fit + navigation | `ViewerController` (shared, unchanged per platform) | same |
| Paged double-page paint | `onPaint` paged branch blits the unit's page HBITMAPs side by side; clamp/pan offsets follow the existing overflow path | `paintEvent` paged branch draws the unit's page QImages; overlay code unchanged |
| Continuous double-page paint | emergent from D3 rects; existing strip loop | emergent from D3 rects; existing loop |
| Keyboard cycle | new case in `onKeyDown` (focus-neutrality guard already applies) | new `QShortcut` + handler |
| Toolbar control | new `ID_*` button in owner-drawn strip, placed after `ID_MODE`, cycling three states | new `QToolButton` after mode toggle, cycling three states |
| Scroll anchoring | uses existing `applyScroll`/`scrollAnchor` | uses existing scrollbar-set path |

## Risks / Trade-offs

- **Singleton pages (cover page 1, odd trailing remainder)** → By design; a unit with a single member is centered on its row, matching single-page behavior. Verified by the `unitLast` helper returning the unit itself when singleton.
- **Cover-aware pairing shifts unit boundaries between the two double-page states** → Switching double-page ↔ double-page-with-cover re-groups pages (e.g. unit (1,2) becomes cover (1) plus (2,3)). That is the intended meaning of the two states: plain double page pairs from 1, cover-aware isolates page 1. Navigation and fit re-resolve against the new rule on switch, so `m_currentPage` is set to `unitFirst(oldPage)` after re-layout.
- **Double-page in continuous yields a much wider canvas than the viewport** → Only the horizontal extent within the unit is wider than one page; the canvas width is bounded by `max(combined unit width, viewport)` via D3, and horizontal scrollbar behavior (`maxScrollOffsetX`) already supports overflow. Mitigation: keep `m_contentSize` width = max unit width so fit-by-width and centering agree.
- **Fit-to-page trade-off in double-page** → Fitting a unit can make each page small on narrow viewports. This is the documented behavior ("fit the unit to the screen"); fit-to-width and manual zoom remain available to trade off.
- **Navigation stepping by 2 may skip a singleton on advance** → `nextPage` steps to the next unit's first page (a cover page 1 is its own singleton unit, so cover mode advances 1 → 2), clamped to the last valid unit, so no page is skipped over or unreachable.
- **Regression risk to single-page behavior** → The presentation state defaults to single-page and every shared path keeps its existing single-page branch; the change is strictly additive. Mitigation: verify single-page renders/scrolls identically after the change (see tasks).
- **Overlay (selection/search) alignment in double-page** → Overlays key off per-page `m_pageRects`, which stay correct; only the paged double-page drawing path must place pages at the same coordinates the overlays expect.

## Migration Plan

No deployment/rollback story beyond a normal build: this is an additive feature in a single plugin binary. Rollback is reverting the change and rebuilding the plugin. No data or dependency migration applies.

## Open Questions

None — grouping (plain vs cover-aware), interaction with all three fit modes, singleton handling, and the toolbar placement are all specified. The specific keyboard key to bind (D7) is a pure key-mapping choice and can be settled during implementation without affecting specs, approach, or tasks.
