## 1. Shared controller: refit fit zoom on paged navigation (D1, D2)

- [x] 1.1 In `src/viewercontroller.h`: add private `void refitAfterNavigation();` to `ViewerController`
- [x] 1.2 Implement `refitAfterNavigation()` in `src/viewercontroller.cpp`: no-op unless `hasDocument() && isPagedMode() && m_fitMode != FitMode::Manual`; capture zoom before `computeFitZoom()`, and only call `computeLayout()` + `notifyChanged()` when the zoom changed by more than ~1e-4f
- [x] 1.3 In paged-mode success paths of `nextPage`, `prevPage`, `firstPage`, `lastPage`, `goToPage` (`src/viewercontroller.cpp`), call `refitAfterNavigation()` after the page changes (not in the continuous-mode branches)
- [x] 1.4 Refactor `computeFitZoom()`: extract the current-unit size computation (the `unitFirst(m_state.currentPage())` / `unitBounds` block) into a small shared `currentUnitSizes()` helper; use it in both the FitToPage and the new paged FitToWidth branch
- [x] 1.5 In the FitToWidth branch of `computeFitZoom()`: when `isPagedMode()`, target the current unit's width (via the extracted helper); keep `maxRowWidth()` only for continuous mode
- [x] 1.6 Ensure manual zoom (`FitMode::Manual`) never refits: `refitAfterNavigation()` returns early; `zoomIn/zoomOut/setManualZoom` already switch to Manual and route through `relayout`

## 2. Platform viewers: single-tap advance tolerance (D3)

- [x] 2.1 In `src/viewer_win32.cpp` `pageBlockDown()`/`pageBlockUp()`: treat a unit as fitting when `maxY <= kPageBlockOverlap` (only `maxY > kPageBlockOverlap` counts as real overflow needing an in-page scroll)
- [x] 2.2 In `src/viewer.cpp` `onPageDown()`/`onPageUp()`: same tolerance guard on the paged branch
- [x] 2.3 Confirm the continuous-mode branches of all four handlers remain untouched (still one-screenful scroll)

## 3. Verification

- [x] 3.1 Add a harness check (extend `tests/harness_win.cpp` or a new `tests/harness_refit.cpp`): with a synthetic mixed-size document (2+ pages with different aspect ratios), open in paged fit-to-page and assert every page's `maxScrollOffsetYForUnit(unitFirst(p)) <= kPageBlockOverlap` right after `pageBlockDown()` navigation — i.e. one PgDn press lands exactly on the next page
- [x] 3.2 Harness check: in paged fit-to-width, the current unit's fitted width equals `pageAreaWidth()` (within rounding) for a narrow unit, while continuous mode still uses the document-wide widest row
- [x] 3.3 Harness check: uniform-size document — navigating pages does not change the zoom (refit is a no-op) and existing behavior is byte-identical
- [x] 3.4 Windows build: `cmake --preset windows-x64-release && cmake --build --preset windows-release` succeeds
- [x] 3.5 Linux build: `cmake --preset linux-release && cmake --build --preset linux-release` succeeds (shared controller + Qt viewer changes compile clean)
- [x] 3.6 Interactive smoke (Windows): open `examples/` mixed-size PDF (e.g. Volvo Handbuch if available, else generate one) maximized, paged + fit-to-page default, verify PgDn advances one page per key press through several pages; switch to fit-to-width and verify a portrait page still scrolls then advances; toggle continuous and verify PgDn scrolls a screenful and zoom does not change
- [x] 3.7 Update `tests/harness_scroll.cpp` G49/G50: paged fit-to-width off the cover now targets the cover (current unit fills the width) and navigating to the spread re-fits it to the width; continuous-mode fit assertions (B3/B4 in `harness_refit.cpp`) confirm the widest-row target is kept in continuous

## 4. Docs

- [x] 4.1 Update `AGENTS.md` Known Issues: record the two-tap PgUp/PgDn root cause (fit zoom anchored on open-time unit) as fixed by paged per-unit refit + the overlap-band tolerance