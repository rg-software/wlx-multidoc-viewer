## 1. Shared State: two-page presentation flag

- [ ] 1.1 Add `bool m_twoPage = false;` plus `setTwoPage(bool)` / `isTwoPage() const` accessors to `ViewerState` (`src/viewerstate.h`), mirroring `m_pagedMode`
- [ ] 1.2 Add `ViewerController` accessors `void toggleTwoPage()` and `bool isTwoPage() const`; `toggleTwoPage` flips the flag, runs `computeLayout()` + `notifyChanged()`
- [ ] 1.3 Verify fresh document open and `closeDocument()` reset `m_twoPage` to `false` (default-constructed `ViewerState` covers both)

## 2. Shared Logic: unit pairing, layout, fit

- [ ] 2.1 Add a shared pairing helper (e.g. `leftPageOfUnit(int page)` returning `((page-1)/2)*2 + 1` and `unitPage(int leftPage)` returning the partner or the page itself when unpaired) on `ViewerController`
- [ ] 2.2 Update `computeLayout()` (`src/viewercontroller.cpp`) so that when `isTwoPage()` the pages are laid out side by side per unit: pair members share the same vertical cursor, the unit height is `max(h_left, h_right)`, unpaired last page is centered, and pages still get individual `m_pageRects` entries
- [ ] 2.3 Update `computeFitZoom()` to fit the whole unit when `isTwoPage()`: fit-to-page uses `min(vw / (w_left + kPageGap + w_right), vh / max(h_left, h_right))`, fit-to-width uses `vw / (w_left + kPageGap + w_right)`, both reading the same `unitBounds` helper used by `computeLayout`
- [ ] 2.4 Update `m_contentSize` width in two-page continuous layout to the max combined unit width (>= viewport), so fit-by-width and centering agree
- [ ] 2.5 Update navigation when two-page: `nextPage()`/`prevPage()` in paged mode step by exactly 2 page indices, `goToPage(n)` resolves to the unit's left page, and `nextPageInContinuousMode`/`prevPageInContinuousMode` advance by one unit (2 indices / 1 unit stride), clamped to the last valid unit

## 3. Shared Toolbar: presentation toggle control

- [ ] 3.1 Add `PresentationToggle` to `toolbar::Control` positioned between `ModeToggle` and `FitButton`, and a matching `toolbar::Icon` for the one-page/two-page glyphs (`src/toolbar.h`)
- [ ] 3.2 Add `ToolbarPresenter::onPresentationToggled()` that calls `controller->toggleTwoPage()` and route it through `refreshState()` so the button checked state and icon always reflect `controller->isTwoPage()`

## 4. Win32 Viewer

- [ ] 4.1 In `ViewerWin32::onPaint` (`src/viewer_win32.cpp`), extend the paged branch: when two-page, blit both pages of the unit side by side (centered as a unit when it fits, panned as a unit when it overflows)
- [ ] 4.2 Add the presentation-toggle keyboard command to `ViewerWin32::onKeyDown` (guarded by the existing edit-focus neutrality check), using an unbound key
- [ ] 4.3 Add the toolbar button in `toolbar_win32.*`: new `ID_*` checkable owner-drawn button placed after `ID_MODE`, wired to `onPresentationToggled()` in `onCommand()`, with `refreshState` pushing checked/icon state

## 5. Qt Viewer

- [ ] 5.1 In `ViewerCanvas::paintEvent` (`src/viewer.cpp`), extend the paged branch: when two-page, draw both pages of the unit side by side with the same centering/pan rules as Win32
- [ ] 5.2 Add the matching `QShortcut` + handler for the presentation toggle in the Qt viewer
- [ ] 5.3 Add the toolbar `QToolButton` in `toolbar_qt.*` after the mode toggle, wired to `onPresentationToggled()`, with lambda-driven checked/icon state from `refreshState`

## 6. Verification

- [ ] 6.1 Configure + build Windows: `cmake --preset windows-x64-release && cmake --build --preset windows-release`
- [ ] 6.2 Configure + build Linux: `cmake --preset linux-release && cmake --build --preset linux-release`
- [ ] 6.3 Smoke test one-page behavior is unchanged (paged, continuous, all three fit modes, wheel/keyboard navigation, selection/search overlays)
- [ ] 6.4 Smoke test two-page in paged mode: pairs (1,2),(3,4)… display side by side, wheel/jog jumps unit-to-unit, odd last page shows alone
- [ ] 6.5 Smoke test two-page in continuous mode: both pages of a unit scroll together, next/prev advances one unit, fit modes fit the combined unit
- [ ] 6.6 Smoke test toolbar/keyboard parity: toggling presentation via the button and via the shortcut stay in sync, and the mode toggle (V) preserves the two-page setting