## 1. Settings header

- [x] 1.1 Create `src/viewer_settings.h` with all tuning constants: `kWheelStepPx` (60), `kKeyboardStepPx` (60), `kScrollBarLineStepPx` (20), `kInfoPanelHeight` (22), `kPageGap` (4), `kPageMargin` (8), `kBufferPages` (3), `kMaxStripHeight` (1500000)
- [x] 1.2 Replace all hardcoded constants in `viewer_win32.cpp` with `#include "viewer_settings.h"` references
- [x] 1.3 Replace `kInfoPanelHeight` in `viewercontroller.cpp` with the settings header constant
- [x] 1.4 Verify build compiles cleanly (no duplicate/inconsistent values)

## 2. Win32 mouse drag

- [x] 2.1 Add drag state fields to `ViewerWin32`: `m_dragging` (bool), `m_lastMouseX`/`m_lastMouseY` (int)
- [x] 2.2 Handle `WM_LBUTTONDOWN` in `handleMsg`: set `m_dragging = true`, record mouse position, call `SetCapture(m_hwnd)`, skip if paged mode
- [x] 2.3 Handle `WM_MOUSEMOVE` in `handleMsg`: if dragging, compute delta from last position, update `m_scrollX`/`m_scrollY`, clamp, call `updateVisiblePage()` + `InvalidateRect`, update last position
- [x] 2.4 Handle `WM_LBUTTONUP` in `handleMsg`: clear `m_dragging`, call `ReleaseCapture()`, check `needsStripRerender()` and re-render if needed
- [x] 2.5 Handle cursor change: override `WM_SETCURSOR` — if dragging, `SetCursor(LoadCursor(NULL, IDC_HAND))` and return `TRUE`; otherwise pass to `DefWindowProc`
- [x] 2.6 Verify drag does not activate in paged mode

## 3. Qt mouse drag + scroll tracking fix

- [x] 3.1 Add drag state fields to `ViewerWidget`: `m_dragging`, `m_lastMousePos`
- [x] 3.2 Override `mousePressEvent`: set dragging, record position, `setCursor(Qt::PointingHandCursor)`, skip if paged mode
- [x] 3.3 Override `mouseMoveEvent`: if dragging, compute delta, update scroll, clamp, repaint
- [x] 3.4 Override `mouseReleaseEvent`: clear dragging, `unsetCursor()`, check `needsStripRerender()` equivalent
- [x] 3.5 Connect `m_scrollArea->verticalScrollBar()->valueChanged(int)` to a slot that calls `m_controller->trackCurrentPage(m_controller->pageAtScrollOffset(scrollY))` — fixes stale `currentPage` during continuous scrolling so toggle and page indicator track the visible page
- [x] 3.6 Verify drag does not activate in paged mode

## 4. Verification

- [x] 4.1 Build both platforms, confirm no warnings
- [x] 4.2 Test: drag in continuous mode scrolls smoothly in both directions
- [x] 4.3 Test: drag in paged mode does nothing
- [x] 4.4 Test: cursor changes to hand during drag, restores on release
- [x] 4.5 Test: drag near document edges clamps correctly
- [x] 4.6 Test: shift+V fit cycle still preserves current page after drag
- [ ] 4.7 Test (Linux): continuous→paged toggle shows top-of-viewport page, not last-navigated page
