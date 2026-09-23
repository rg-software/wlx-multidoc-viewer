## 1. Measurement spike

- [ ] 1.1 Add `tests/harness_djvu.cpp` and a `harness-djvu` target in `CMakeLists.txt` that opens a DjVu and times `open`, `pageDimensions(1..N)`, and `renderPage(1)`, printing per-phase milliseconds.
- [ ] 1.2 Run the harness on a real large DjVu (hundreds of pages) and on `examples/sample1.djvu`; record open time, total measure-all time, per-page measure average, and first-render time.
- [ ] 1.3 Decide the scheduler from the data: if total measure-all is small (roughly under 1 s) prefer lock-free UI-thread idle batching; otherwise keep the completion worker (design D4). Record the per-page threshold that decides whether the UI prefetch needs a try-lock.

## 2. Shared engine (DjVu)

- [ ] 2.1 Add `virtual bool prefersLazyPageSizing() const { return false; }` to `DocumentEngine`; override to `true` in `DjVuEngine`.
- [ ] 2.2 Guard all `DjVuEngine` public entry points (`open`, `close`, `renderPage`, `pageDimensions`, `pageCount`) with a `std::mutex` so a measurement worker and the UI never pump the context concurrently.
- [ ] 2.3 Verify `pageDimensions` returns the cached result on repeat and does not decode the page (spec `djvu-renderer`).

## 3. Shared controller

- [ ] 3.1 Split `computeLayout` into `buildLayoutFromSizes(sizes)` (pure geometry, no cache clear, no epoch bump) and `invalidateLayout()`; route existing zoom/rotation/DPI/fit changes through `invalidateLayout`.
- [ ] 3.2 Track measured vs provisional page sizes; make `computeFitZoom` and the initial layout use measured sizes for the current unit and provisional sizes elsewhere when `prefersLazyPageSizing()` is true.
- [ ] 3.3 Make `openDocument` measure only the current view unit for lazy engines; keep the eager all-page loop for non-lazy engines.
- [ ] 3.4 Implement UI-thread prefetch: measure pages in `[firstVisible - buffer, lastVisible + buffer]` before composing, then refine.
- [ ] 3.5 Implement refinement: update the page size, run `buildLayoutFromSizes`, re-anchor the scroll (reuse the `relayout` capture/restore), repaint; do not clear caches.
- [ ] 3.6 Handle the continuous fit-to-width re-fit on refinement with the `refitAfterNavigation` epsilon guard; invalidate caches only when the zoom actually changed.
- [ ] 3.7 Add the completion worker (`std::thread` plus cancel/join, marshaled via `setUiMarshal`) measuring remaining pages in batches; cancel/join in `closeDocument` and on document swap.

## 4. Platform integration

- [ ] 4.1 Win32: confirm refinement notifications repaint through the existing `setUiMarshal` PostMessage path and that scroll position updates after refinement.
- [ ] 4.2 Qt: confirm refinement notifications repaint through the existing queued `setUiMarshal` path and that the scrollbar range updates after refinement.

## 5. Verification

- [ ] 5.1 Windows `cmake --preset windows-x64-release && cmake --build --preset windows-release` and Linux `cmake --preset linux-release && cmake --build --preset linux-release` both succeed.
- [ ] 5.2 Harness: a large DjVu measures O(1) pages before first paint; `pageCount()` is exact at open; `goToPage(N)` works immediately; after refinement the range converges and geometry matches the eager path.
- [ ] 5.3 Regression: `harness-refit`, `harness-scroll`, `harness-startup`, and `harness-links` still pass; MuPDF/comic/image open paths unchanged.
- [ ] 5.4 Interactive smoke on a large DjVu: fast open; continuous scroll across the refined boundary shows no viewport jump and no re-render flash; zoom in/out and rotate stay anchored; jump to the last page works.
