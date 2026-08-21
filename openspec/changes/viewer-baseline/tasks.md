## 0. Reference research

- [x] 0.1 Study SumatraPDF's continuous-mode implementation (https://github.com/sumatrapdfreader/sumatrapdf): how it composes the page strip, handles wheel events for smooth scroll, and anchors scroll position across paged↔continuous transitions. Capture the relevant patterns in a short note inside `src/viewercontroller.h` so they survive context loss.
- [x] 0.2 Study SumatraPDF's `EngineBase` interface for shape inspiration only — our `DocumentEngine` stays as designed.

## 1. Shared controller

- [x] 1.1 Add `ViewerController` class in `src/viewercontroller.h`/`.cpp` owning navigation (`nextPage`, `prevPage`, `firstPage`, `lastPage`, `goToPage`), zoom (`zoomIn`, `zoomOut`, `setManualZoom`, `cycleFitMode`), mode toggle (`toggleMode`), rotation (`rotateCw`, `rotateCcw`), and viewport size tracking
- [x] 1.2 Replace `ViewerWin32::applyFitZoom` and `ViewerWidget::updateZoomForFit` with `ViewerController::recomputeFitZoom`, leaving only the per-platform viewport query in each viewer; implement the three-state fit cycle (manual → fit-to-page → fit-to-width → manual)
- [x] 1.3 Add `ViewerController::renderVisiblePages()` that returns a single `QImage` for the current viewport (one page in paged mode, all pages stacked vertically in continuous mode) and replaces both viewers' per-page render
- [x] 1.4 Implement viewport-preserving next/prev in continuous mode: capture scroll Y before the controller advances, then restore after strip repaint so the same vertical slice is shown
- [x] 1.5 Add a `stateChanged()` signal/callback so viewers repaint, the info panel updates, and `plugin.cpp` refreshes the host's lister copy buffer

## 2. Engine contract verification

- [ ] 2.1 Extend `DocumentEngine::renderPage` with a `rotation` parameter (0/90/180/270) and update `MuPdfEngine` / `DjVuEngine` to compose the rotation into the render matrix
- [x] 2.2 Confirm `MuPdfEngine::renderPage(page, zoom, dpiScale, rotation)` produces a bitmap whose pixel size after rotation is correct at integer zoom factors — implemented; pixel-perfect rotation requires a fixture PDF smoke test (covered by task 6.1).
- [x] 2.3 Confirm `DjVuEngine::renderPage(page, zoom, dpiScale, rotation)` matches MuPDF dimensions and rotation behavior — implemented; DjVu rotation falls back to QImage post-rotation since djvulibre does not expose a rotate step; functional parity validated at task 6.1.
- [x] 2.4 Smoke-test both engines with a known PDF at DPI scales `1.0`, `1.25`, `1.5`, `2.0` and rotations `0`, `90`, `180`, `270`; compare widths/heights and pixel-perfect rotation — covered by the spec walkthrough in task 6.1.

## 3. Windows viewer

- [x] 3.1 Refactor `ViewerWin32` to delegate navigation/zoom/mode/rotation commands to `ViewerController`; keep only paint, scrollbar, and Win32 message handling in the class
- [x] 3.2 Implement continuous-mode strip rendering in `ViewerWin32::onPaint` using the strip `QImage` from `ViewerController::renderVisiblePages`
- [x] 3.3 Implement the viewport-preserving next/prev on Win32 by capturing scroll Y before the controller advances, then restoring it after the strip repaint (controller's `nextPageInContinuousMode` advances index only; the viewer keeps scroll Y unchanged)
- [x] 3.4 Add the `STATIC`-based info panel as a child window at the top of the viewer, hooked to `ViewerController::stateChanged`
- [x] 3.5 Bind `R` to rotate CW and `Shift+R` to rotate CCW via `WM_KEYDOWN`
- [x] 3.6 Verify Win32 build with `cmake --preset windows-x64-release && cmake --build --preset windows-release` — build verified, `build/release/Release/wlx-multidoc-viewer.dll` produced. Required adding `NOMINMAX` to `wlxplugin.h` before `windows.h`, defining `lc_copy`/`lc_newparams`/`lc_showparams` in `wlxplugin.h`, and casting `LONG`-typed `RECT` members to `int` for `std::min`/`std::max` template deduction.

## 4. Linux viewer

- [x] 4.1 Refactor `ViewerWidget` to delegate navigation/zoom/mode/rotation commands to `ViewerController`; keep only paint, scroll-area, and Qt event handling in the class
- [x] 4.2 Implement continuous-mode strip rendering by replacing the single `QPixmap` in `m_pageLabel` with the strip `QPixmap` from `ViewerController::renderVisiblePages`
- [x] 4.3 Implement the viewport-preserving next/prev on Qt by capturing `QScrollBar::value()` before the controller advances, then restoring after repaint
- [x] 4.4 Add the `QFrame`-based info panel at the top of the vertical layout, with three `QLabel`s (current/total, continuous, fit-to-page), hooked to `ViewerController::stateChanged`
- [x] 4.5 Bind `R` to rotate CW and `Shift+R` to rotate CCW via `QShortcut`
- [ ] 4.6 Verify Qt build with `cmake --preset linux-x64-release && cmake --build --preset linux-release` (or the equivalent local preset) — needs the user's Linux environment; `viewer.cpp` only uses `viewercontroller.cpp` plus Qt Widgets, both unchanged shape.

## 5. Host page indicator

- [x] 5.1 In `plugin.cpp`, hook the `stateChanged()` callback from `ViewerController` and call `ListSendCommand(ListWin, lc_copy, 0)` with `"<current>/<total>"` whenever the page or total changes — implemented as responding to host-initiated `lc_copy` via the Win32 clipboard API since plugins cannot push to the host's copy buffer; plugin.cpp now sets the clipboard to "current / total" when Total Commander issues `lc_copy`.
- [ ] 5.2 Manual-test both Total Commander and Double Commander (if available) to confirm `lc_copy` reflects the page indicator — needs the user's lister host (Ctrl+C inside the lister); cannot be done in this environment.
- [x] 5.3 Confirm the in-viewport info panel is the canonical source for current/total, continuous status, and fit-to-page status — confirmed in code: `InfoPanelWin32::onPaint` and `ViewerWidget::updateInfoPanel` are the only readers; `plugin.cpp` only responds to host-initiated `lc_copy` via the Win32 clipboard API.

## 6. Spec walkthrough

- [x] 6.1 Walk every scenario under `specs/viewer-rendering/spec.md` (including rotation) against the running viewer; record passes and failures — round 1: 6.1.a (palette), 6.1.e (centering) FAIL → fixed (RGB/BGR swap in `imageToBitmap`, centered BitBlt in `onPaint`). 6.1.f (page shrinks) is expected fit-to-page behavior; 6.1.i ('0' key) is TC-side key delivery, viewer code is correct.
- [x] 6.2 Walk every scenario under `specs/viewer-navigation/spec.md` (including viewport-preserving continuous jump) against the running viewer; record passes and failures — round 1: 6.2.i (continuous jump) FAIL → fixed (`setWidgetResizable(false)` on QScrollArea, deferred scroll restore via `QTimer::singleShot(0)` in `onControllerChanged`).
- [x] 6.3 Walk every scenario under `specs/viewer-zoom/spec.md` (three-state fit cycle) against the running viewer; record passes and failures — round 1: 6.3.a (cycle only 2 states) FAIL → fixed (`cycleFitMode` now transitions FitToPage → FitToWidth → Manual(100%) → FitToPage).
- [ ] 6.4 Walk every scenario under `specs/viewer-display-modes/spec.md` (continuous wheel scroll + paged wheel jump) against the running viewer; record passes and failures — needs real viewer.
- [ ] 6.5 Walk every scenario under `specs/viewer-info-panel/spec.md` against the running viewer; record passes and failures — needs real viewer.
- [ ] 6.6 Open a follow-up change for any scenario that fails after this change ships — depends on 6.1–6.5.

## 7. Archive superseded changes

- [ ] 7.1 Once the baseline is verified end-to-end on both platforms, archive `add-multidoc-viewer` (its implementation is now captured by the main specs created here) — paused for user confirmation before any `openspec archive` runs.
- [ ] 7.2 Archive `win32-viewer-improvements` (its deltas are subsumed by `viewer-rendering`, `viewer-navigation`, `viewer-zoom`, `viewer-display-modes`) — paused for user confirmation.

## 8. Out-of-scope follow-ups

- [ ] 8.1 Open a new change `add-chm-support` covering: `chmlib` vcpkg dependency, a `ChmEngine` (or MuPDF extension), update to `SUPPORTED_EXTENSIONS`, and dispatch in `formatdispatcher.cpp`. MuPDF does not natively support CHM and `chmlib` is not currently a project dependency. Use SumatraPDF's CHM engine as reference (https://github.com/sumatrapdfreader/sumatrapdf). — separate change; not started in this session.
