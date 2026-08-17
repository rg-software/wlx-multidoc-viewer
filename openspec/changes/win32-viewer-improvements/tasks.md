## 1. Shared Viewer Logic

- [ ] 1.1 Create `src/viewerstate.h` with page state (currentPage, pageCount, zoom, fitToWidth, pagedMode), navigation methods, and zoom calculation formulas
- [ ] 1.2 Implement page navigation: nextPage(), prevPage(), firstPage(), lastPage() with bounds checking
- [ ] 1.3 Implement zoom calculation: fitToWidthZoom(pageW, viewportW), fitToPageZoom(pageW, pageH, viewportW, viewportH), zoomIn(), zoomOut()
- [ ] 1.4 Implement mode toggle: setPagedMode(bool), isPagedMode()

## 2. DPI-Aware Engine Rendering

- [ ] 2.1 Add DPI scale factor parameter to `DocumentEngine::renderPage()` (default 1.0 for backward compat)
- [ ] 2.2 In MuPdfEngine, multiply the CTM by the DPI scale factor before rendering
- [ ] 2.3 In DjVuEngine, apply DPI scale to the render call
- [ ] 2.4 Verify QImage→HBITMAP conversion (Win32) and QImage display (Qt) preserve quality at scaled DPI

## 3. Win32 Viewer Integration

- [ ] 3.1 Include `viewerstate.h` in ViewerWin32, replace ad-hoc state with ViewerState methods
- [ ] 3.2 Fix `onSize` to use shared fitToWidthZoom/fitToPageZoom formulas
- [ ] 3.3 Add keyboard handler delegating to ViewerState navigation methods
- [ ] 3.4 Add DPI detection: GetDpiForWindow with fallback, pass dpi/96.0 to engine renderPage
- [ ] 3.5 Add page indicator: DrawText "current/total" in WM_PAINT
- [ ] 3.6 Implement paged mode: single page paint, scroll-past-bottom advances page
- [ ] 3.7 Implement continuous mode: composite visible pages vertically, scroll offset into composite

## 4. Qt Viewer Integration

- [ ] 4.1 Include `viewerstate.h` in ViewerWidget, replace ad-hoc state with ViewerState methods
- [ ] 4.2 Fix `resizeEvent` to use shared fitToWidthZoom/fitToPageZoom formulas
- [ ] 4.3 Add keyboard handler delegating to ViewerState navigation methods
- [ ] 4.4 Add DPI detection: QScreen::logicalDotsPerInch, pass dpi/96.0 to engine renderPage
- [ ] 4.5 Add page indicator: update QLabel with "current/total" on navigation
- [ ] 4.6 Implement paged mode: single page in scroll area, scroll-past-bottom advances page
- [ ] 4.7 Implement continuous mode: render visible pages into a composite QWidget, scroll through it

## 5. Build & Verify

- [ ] 5.1 Add viewerstate.h to CMakeLists.txt (header-only or compiled into both targets)
- [ ] 5.2 Build and test on Windows: navigation, resize, DPI, modes
- [ ] 5.3 Build and test on Linux: navigation, resize, DPI, modes
