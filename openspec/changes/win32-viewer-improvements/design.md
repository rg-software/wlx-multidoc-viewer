## Context

The Win32 viewer (`viewer_win32.cpp`) currently renders a single page into an HBITMAP and paints it via BitBlt. There is no page tracking, no zoom recalculation on resize, and no DPI awareness. The MuPDF and DjVuLibre engines both accept a zoom float and return a QImage, which the viewer converts to a DIB section for display.

## Goals / Non-Goals

**Goals:**
- Add page navigation state (current page, total pages) with keyboard shortcuts
- Fix resize behavior: recalculate zoom to maintain fit-to-width without distortion
- Make rendering DPI-aware using `GetDpiForWindow` / `GetDeviceCaps`
- Add paged and continuous reading modes
- Keep the viewer a pure Win32 child HWND with no Qt widget dependency

**Non-Goals:**
- Text selection or copy/paste (future enhancement)
- Table of contents / outline panel (future enhancement)
- Printing support
- Search within document

## Decisions

### 1. Page state lives in the viewer, not the engine
**Choice**: Track `m_currentPage` and page count in `ViewerWin32`, query the engine for total count on open.
**Rationale**: Engines are stateless renderers. Keeping navigation in the viewer avoids coupling and allows the same engine to serve different view modes.

### 2. Zoom as a float multiplier, not a percentage
**Choice**: Store zoom as `float m_zoom` (1.0 = 100%). Fit-to-width sets zoom = viewport_width / page_width_at_100%.
**Rationale**: The engines already accept a float zoom. No new API needed. Zoom In/Out multiply by 1.25/0.8.

### 3. DPI via GetDpiForWindow
**Choice**: On Windows 10 1607+, call `GetDpiForWindow(m_hwnd)` to get the actual DPI. Scale the render zoom by `dpi / 96.0`.
**Rationale**: `GetDpiForWindow` is the modern per-window DPI API. Falls back to `GetDeviceCaps(hdc, LOGPIXELSX)` on older Windows. The engine renders at the DPI-scaled resolution, so text is sharp.

### 4. Continuous mode: render all visible pages into one bitmap
**Choice**: For continuous mode, render each visible page and composite them vertically into a single tall HBITMAP. Scroll position is a pixel offset into this composite.
**Rationale**: Simple approach that reuses the existing single-page render. Only renders pages intersecting the viewport (lazy). The composite bitmap is rebuilt on scroll or resize.

### 5. Paged mode: single page, scaled to fit
**Choice**: In paged mode, calculate zoom to fit the page within the viewport (respecting aspect ratio). Scroll past bottom advances to next page.
**Rationale**: Standard document viewer behavior. The fit calculation uses `min(viewport_w / page_w, viewport_h / page_h)`.

### 6. WM_SIZE triggers full re-render
**Choice**: On `WM_SIZE`, recalculate zoom (if fit-to-width/fit-to-page is active), re-render the current page(s), and update scroll bars.
**Rationale**: The current code already does this but with incorrect zoom math. The fix is to use `page_dimensions` from the engine to compute the correct zoom factor.

## Risks / Trade-offs

- **[Risk] Continuous mode memory usage** → Mitigation: Only render pages visible in the viewport + 1 page buffer above/below. Discard off-screen bitmaps.
- **[Risk] GetDpiForWindow unavailable on Windows 7** → Mitigation: Fallback to 96 DPI. The plugin still works, just not HiDPI-sharp.
- **[Trade-off] Re-render on every resize** → Acceptable for now. Could add debouncing (100ms timer) later if resize performance is poor on large documents.
- **[Trade-off] No anti-aliasing in continuous mode gaps** → Simple solid-color gap between pages. Could add shadows later.
