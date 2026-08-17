## Context

Both viewers (Win32 on Windows, Qt on Linux) currently render a single static page with incorrect zoom math. The document engines (MuPDF, DjVuLibre) are already cross-platform and accept a zoom float, returning a QImage. The goal is to share viewer logic (navigation state, zoom calculation, mode switching) and keep platform code to a minimum.

## Goals / Non-Goals

**Goals:**
- Add page navigation state (current page, total pages) with keyboard shortcuts — shared logic
- Fix resize behavior: recalculate zoom to maintain fit-to-width without distortion — shared math
- Make engine rendering DPI-aware — engine layer change, benefits both platforms
- Add paged and continuous reading modes — shared state, platform-specific scroll/paint
- Keep platform-specific code minimal: only HWND vs Qt widget plumbing

**Non-Goals:**
- Text selection or copy/paste (future enhancement)
- Table of contents / outline panel (future enhancement)
- Printing support
- Search within document

## Decisions

### 1. Shared viewer logic in a common module
**Choice**: Extract page state, zoom math, and mode state into a shared `ViewerState` class or header that both viewers include. Each platform implements only paint/scroll/keyboard handling.
**Rationale**: Avoids duplicating navigation bounds checks, zoom calculations, and mode toggling across two viewers. The shared code is pure logic with no platform dependencies.

### 2. Page state lives in the viewer, not the engine
**Choice**: Track `m_currentPage` and page count in each viewer, query the engine for total count on open.
**Rationale**: Engines are stateless renderers. Keeping navigation in the viewer avoids coupling and allows the same engine to serve different view modes.

### 3. Zoom as a float multiplier, not a percentage
**Choice**: Store zoom as `float m_zoom` (1.0 = 100%). Fit-to-width sets zoom = viewport_width / page_width_at_100%.
**Rationale**: The engines already accept a float zoom. No new API needed. Zoom In/Out multiply by 1.25/0.8. Both viewers use the same formula.

### 4. DPI via platform APIs
**Choice**: Windows: `GetDpiForWindow()` with `GetDeviceCaps()` fallback. Linux: `QScreen::logicalDotsPerInch()`. Multiply engine render zoom by `dpi / 96.0`.
**Rationale**: Each platform has its own DPI API. The engine doesn't need to know — it just receives a DPI-scaled zoom factor from the viewer.

### 5. Continuous mode: render visible pages into one buffer
**Choice**: For continuous mode, render each visible page and composite them vertically into a single buffer. Scroll position is a pixel offset into this composite.
**Rationale**: Simple approach that reuses the existing single-page render. Only renders pages intersecting the viewport (lazy). Platform code handles the buffer → screen blit (BitBlt on Win32, QPainter on Qt).

### 6. Paged mode: single page, scaled to fit
**Choice**: In paged mode, calculate zoom to fit the page within the viewport (respecting aspect ratio). Scroll past bottom advances to next page.
**Rationale**: Standard document viewer behavior. The fit calculation uses `min(viewport_w / page_w, viewport_h / page_h)` — shared math.

### 7. Platform-specific code is limited to
- **Win32**: `WM_PAINT` / `BitBlt` / `CreateDIBSection` for painting, `WM_VSCROLL` / `WM_HSCROLL` for scrolling, `GetDpiForWindow` for DPI
- **Qt**: `paintEvent` / `QPainter` for painting, `QScrollArea` for scrolling, `QScreen::logicalDotsPerInch` for DPI

Everything else (page state, zoom formula, mode toggle logic, bounds checking) is shared.

## Risks / Trade-offs

- **[Risk] Shared module adds build complexity** → Mitigation: Header-only or a single `.cpp` compiled into both targets. CMakeLists.txt conditionally links it.
- **[Risk] Continuous mode memory usage** → Mitigation: Only render pages visible in the viewport + 1 page buffer above/below. Discard off-screen bitmaps.
- **[Trade-off] DPI calculation differs per platform** → Acceptable. Each viewer queries its own DPI and passes the scaled zoom to the shared engine.
- **[Trade-off] Re-render on every resize** → Acceptable for now. Could add debouncing (100ms timer) later if resize performance is poor on large documents.
