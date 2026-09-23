## Why

Opening large DjVu documents takes 7-10 seconds because the viewer measures every page's dimensions synchronously on the UI thread before the first paint. Each measurement forces DjVuLibre to instantiate and parse that page's IFF structure, so open time scales with page count (see issue #19).

## What Changes

- Open with only the current view unit measured; build the continuous-mode layout from an estimated page geometry instead of measuring every page up front.
- Measure the remaining page dimensions progressively after the first paint, and refine each page's geometry once its real size is known (mechanism chosen in design after a timing spike).
- Refine geometry without clearing the render cache and without moving the page under the viewport.
- Keep page count and direct page jump exact and available immediately: the count comes from the DjVu document directory, not from page dimensions.
- Add a DjVu timing harness that quantifies open / measure-all-pages / first-render cost before and after.

## Capabilities

### New Capabilities
- `djvu-renderer`: DjVu page count read from the document directory; demand-driven, cached page-dimension measurement that never forces a full-page decode; document usable immediately at open.

### Modified Capabilities
- `viewer-scrolling`: continuous layout may start from estimated page geometry and converge to exact per-page geometry through anchored refinement, with no visual jump and no render-cache invalidation for unchanged pages.
- `viewer-navigation`: page count and direct page jump are available as soon as the document opens, independent of dimension measurement.

## Impact

- `src/viewercontroller.cpp` (layout, relayout, refinement), `src/djvuengine.*` (measurement behavior/API), both viewers for refinement notification.
- New `tests/harness_djvu.cpp` plus a `harness-djvu` target in `CMakeLists.txt`.
- No new dependency; DjVuLibre is retained. `kjk/djvudec` is evaluated only if the spike shows decoding, not measurement, dominates.
- Windows (Win32) and Linux (Qt) both affected: the layout logic is shared in `ViewerController`.
