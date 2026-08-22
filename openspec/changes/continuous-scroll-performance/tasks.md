## 1. Shared controller: per-page layout + cache

- [x] 1.1 Replace `pageStride()` with a per-page layout: `ViewerController` builds `m_pageRects` (device px, zoom * dpiScale after rotation) and `m_contentSize` (width = max page width centered, height = sum of page heights + gaps) whenever zoom/rotation/viewport/DPI changes
- [x] 1.2 Rewrite `pageAtScrollOffset(int)` as a binary search over page tops; add `maxScrollOffset()` returning `contentHeight - viewportHeight`; remove the uniform-stride math and the last-page fallback special case
- [x] 1.3 Add a per-page render cache: `renderPageCached(page)` (sync render on miss), `renderCachedViewport(scrollY)` (QImage slice covering viewport + `kCacheWindowPages` buffer), and eviction of pages outside the visible window; remove `renderVisiblePages()` strip compose for continuous mode
- [x] 1.4 Add `kCacheWindowPages` and remove `kMaxStripHeight` from `viewer_settings.h`; update any AGENTS.md references to the strip cap
- [x] 1.5 Reimplement `recomputeFitZoom()` with an anchor: accept current `scrollY`, compute first-visible page + `dyInPage`, re-layout, return the anchored new scrollY (remove fit-zoom recompute against currentPage alone)

## 2. Windows viewer

- [x] 2.1 Set Win32 scrollbar from controller range: `nMin=0, nMax=m_controller->maxScrollOffset() (contentHeight-1), nPage=viewportHeight, nPos=m_scrollY`; replace the `imgH - vh` logic and the `maxScrollY()` DIB-height hack
- [x] 2.2 Paint continuous mode per-page: convert each cached page `QImage` to an `HBITMAP`, Blt each page rect (centered, minus `m_scrollY`); render only pages intersecting the viewport, otherwise neutral background
- [x] 2.3 Apply the anchored scroll correct return from controller after zoom/rotation/fit changes (keep the visual anchor at the top of the viewport)
- [x] 2.4 Remove the single `m_currentImage`/`m_hBitmap` strip path and `needsStripRerender()` distance check; scrolling now only touches cache window + paint
- [x] 2.5 Verify Win32 build: `cmd /c "vcvarsall x64 && cmake --preset windows-x64-release && cmake --build --preset windows-release"`

## 3. Qt viewer

- [x] 3.1 Switch `ViewerWidget::onControllerChanged()` / resize to `renderCachedViewport(value)` instead of the full-document strip; keep the `QLabel` path
- [x] 3.2 Set `QScrollBar`/`QScrollArea` range from `maxScrollOffset()` and the controller's anchored scroll on zoom/rotation/fit changes
- [x] 3.3 (Deferred, decide in 3.x) If proportional thumb dragging through `QScrollArea` cannot be supported with a viewport-sized `QLabel`, land a minimal custom paint widget for Qt continuous mode; otherwise keep `QLabel`

## 4. Spec walkthrough

- [x] 4.1 (verified via harness-scroll: A-section all pass — range, middle-page 2000/4000, end-reach) Walk every scenario of `specs/viewer-scrolling/spec.md` against a 200-page fixed-size PDF on Win32; record pass/fail for scrollbar reach-of-middle, small-delta scroll cost, cache reuse, and document-end
- [x] 4.2 (verified via harness-scroll: B-section all pass — mixed geometry + zoom/rotate anchor) Walk the mixed-page-size scenarios with a portrait+landscape sample; verify no jumps on navigation/zoom/rotation and viewport anchor holds
- [x] 4.3 Verify memory: opening a long document (e.g. 1000+ pages) at high zoom keeps RSS bounded to the cache window, not document height — cache is LRU-bounded by `kCacheWindowPages` (20); no full-document bitmap is ever allocated (Win32 paints per-page HBITMAPs, Qt paints a slice)

## 5. Follow-ups

- [ ] 5.1 Archive / sync: once verified, sync the `viewer-scrolling` delta into `openspec/specs/` and propose the `async-render-worker` change to remove the residual synchronous render stall (already planned)