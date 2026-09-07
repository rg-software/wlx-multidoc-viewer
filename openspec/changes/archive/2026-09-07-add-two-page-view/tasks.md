## 1. Shared State: page-presentation enum

- [x] 1.1 Add a `PagePresentation` enum (`Single`, `Double`, `DoubleWithCover`) and a member defaulting to `Single` plus `setPagePresentation(PagePresentation)` / `pagePresentation() const` accessors to `ViewerState` (`src/viewerstate.h`), mirroring `m_pagedMode`
- [x] 1.2 Add `ViewerController` accessors `void cyclePagePresentation()` and `PagePresentation pagePresentation() const`; `cyclePagePresentation` advances single → double → double-with-cover → single, re-resolves `m_currentPage` to `unitFirst(m_currentPage)` under the new state, runs `computeLayout()` + `notifyChanged()`
- [x] 1.3 Verify fresh document open and `closeDocument()` reset the presentation to `Single` (default-constructed `ViewerState` covers both)

## 2. Shared Logic: unit pairing, layout, fit

- [x] 2.1 Add shared pairing helpers on `ViewerController`: `unitFirst(int page)` (first page of the unit containing `page`, cover-aware or plain per presentation) and `unitLast(int unitFirst)` (the unit's second member, or `unitFirst` itself when the unit is singleton)
- [x] 2.2 Update `computeLayout()` (`src/viewercontroller.cpp`) so that when the presentation is `Double` or `DoubleWithCover` the units are laid out side by side: unit members share the same vertical cursor, the unit height is `max(h_first, h_last)`, a singleton unit (cover page 1 or an unpaired last page) is centered on its row, and pages still get individual `m_pageRects` entries
- [x] 2.3 Update `computeFitZoom()` to fit the whole unit when the presentation is `Double` or `DoubleWithCover`: fit-to-page uses `min(vw / combinedWidth, vh / max(h_first, h_last))`, fit-to-width uses `vw / combinedWidth`, where `combinedWidth = w_first + (last != first ? kPageGap + w_last : 0)`, both reading the same `unitBounds` helper used by `computeLayout`
- [x] 2.4 Update `m_contentSize` width in double-page continuous layout to the max combined unit width (>= viewport), so fit-by-width and centering agree
- [x] 2.5 Update navigation when the presentation is `Double` or `DoubleWithCover`: `nextPage()`/`prevPage()` in paged mode step unit to unit (cover mode steps 1 → 2, 2 → 4, …; plain mode steps 1 → 3, …), `goToPage(n)` resolves to `unitFirst(n)`, and `nextPageInContinuousMode`/`prevPageInContinuousMode` advance by one unit, clamped to the last valid unit
- [x] 2.6 Add `hasNextPage()`/`hasPrevPage()` (adjacent *unit* tests, so the final unit disables next while `currentPage < pageCount`) and `scrollStepPage(base, ±1)` (continuous screen step: whole unit per row in double-page states, raw page step in single) to `ViewerController`; the toolbar uses both, and harness G52–G61 cover paged enablement, unit-aware continuous stepping, and no-op edges

## 3. Shared Toolbar: presentation control

- [x] 3.1 Add `PresentationToggle` to `toolbar::Control` positioned between `ModeToggle` and `FitButton`, and `toolbar::Icon` glyphs for the three states (single / double / double-with-cover) (`src/toolbar.h`)
- [x] 3.2 Add `ToolbarPresenter::onPresentationCycled()` that calls `controller->cyclePagePresentation()` and route it through `refreshState()` so the control state and icon always reflect `controller->pagePresentation()` (three-state cycle)
- [x] 3.3 Route prev/next enablement and clicks through `hasPrevPage()`/`hasNextPage()` (paged) and `scrollStepPage` at the scroll anchor (continuous), so the buttons stay disabled when their step cannot move, and re-carve the embedded icon font with the display-mode/fit/presentation glyphs as real Material Symbols codepoints (paged → `check_box_outline_blank` 0xe835, fit manual → `pinch` 0xeb38, fit page → `fit_page` 0xf77a, fit width → `fit_width` 0xf779, presentation single → `article` 0xef42, presentation double → `two_pager` 0xf51f, presentation double-with-cover → `chrome_reader_mode` 0xe86d; `ModeContinuous` keeps the font `view_agenda` glyph)
- [x] 3.4 Sync the scroll anchor (and refresh the toolbar) at the end of Win32 `applyScroll` and in the Win32 key-down continuous paths; Qt syncs in `onVerticalScrollChanged` with a toolbar refresh — fixes continuous Prev/Next enablement lagging one gesture behind after go-to/jump

## 4. Win32 Viewer

- [x] 4.1 In `ViewerWin32::onPaint` (`src/viewer_win32.cpp`), extend the paged branch: when the presentation is `Double` or `DoubleWithCover`, blit the unit's pages side by side (centered as a unit when it fits, panned as a unit when it overflows)
- [x] 4.2 Add the presentation-cycle keyboard command to `ViewerWin32::onKeyDown` (guarded by the existing edit-focus neutrality check), using an unbound key
- [x] 4.3 Add the toolbar control in `toolbar_win32.*`: a new `ID_*` owner-drawn BUTTON placed after `ID_MODE` that cycles through the three states on each click, wired to `onPresentationCycled()` in `onCommand()`, with `refreshState` pushing state/icon

## 5. Qt Viewer

- [x] 5.1 In `ViewerCanvas::paintEvent` (`src/viewer.cpp`), extend the paged branch: when the presentation is `Double` or `DoubleWithCover`, draw the unit's pages side by side with the same centering/pan rules as Win32
- [x] 5.2 Add the matching `QShortcut` + handler for the presentation cycle in the Qt viewer
- [x] 5.3 Add the toolbar `QToolButton` in `toolbar_qt.*` after the mode toggle, wired to `onPresentationCycled()`, cycling the three states with lambda-driven state/icon from `refreshState`

## 6. Verification

- [x] 6.1 Configure + build Windows: `cmake --preset windows-x64-release && cmake --build --preset windows-release`
- [ ] 6.2 Configure + build Linux: `cmake --preset linux-release && cmake --build --preset linux-release`
- [ ] 6.3 Smoke test single-page behavior is unchanged (paged, continuous, all three fit modes, wheel/keyboard navigation, selection/search overlays)
- [ ] 6.4 Smoke test `Double`: pairs (1,2),(3,4)… display side by side, wheel/jog jumps unit-to-unit, odd last page shows alone
- [ ] 6.5 Smoke test `DoubleWithCover`: page 1 shows alone as its own unit, then (2,3),(4,5)… pair; next/prev and go-to honor the cover-aware boundaries; odd last page shows alone
- [ ] 6.6 Smoke test double-page states in continuous mode: both pages of a unit scroll together, next/prev advances one unit, fit modes fit the combined unit
- [ ] 6.7 Smoke test toolbar/keyboard parity: cycling presentation via the control and via the shortcut stay in sync, the control reflects all three states, and the mode toggle (V) preserves the presentation setting