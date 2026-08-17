## 1. Page Navigation

- [ ] 1.1 Add page state members to ViewerWin32: `m_currentPage`, `m_pageCount`, update on `loadDocument`
- [ ] 1.2 Add keyboard handler for Right/Left/Home/End/PageUp/PageDown to navigate pages
- [ ] 1.3 Add page bounds checking: prevent navigation past first/last page
- [ ] 1.4 Render current page and update a text-based page indicator ("1/5") via DrawText in WM_PAINT

## 2. Correct Resize & Zoom

- [ ] 2.1 Fix `onSize` to compute zoom from `page_dimensions` and viewport size, not from raw pixel math
- [ ] 2.2 Ensure aspect ratio is preserved: use `min(vw/pw, vh/ph)` for fit-to-page, `vw/pw` for fit-to-width
- [ ] 2.3 Add DPI detection: call `GetDpiForWindow` (Win10 1607+) with fallback to `GetDeviceCaps`, multiply render zoom by `dpi/96.0`
- [ ] 2.4 Re-render and invalidate on every `WM_SIZE` event

## 3. Sharp Text Rendering

- [ ] 3.1 Verify MuPDF engine uses `fz_scale(zoom * dpiScale, zoom * dpiScale)` in the CTM passed to `fz_new_pixmap_from_page`
- [ ] 3.2 Verify DjVu engine applies DPI scale to the render call
- [ ] 3.3 Confirm QImage→HBITMAP conversion preserves 24-bit RGB without quantization artifacts

## 4. Paged Mode

- [ ] 4.1 Implement paged mode: single page scaled to fit viewport (aspect-preserving)
- [ ] 4.2 Handle scroll-past-bottom to advance to next page
- [ ] 4.3 Handle scroll-past-top to go back to previous page

## 5. Continuous Mode

- [ ] 5.1 Add continuous mode state flag and Ctrl+N toggle
- [ ] 5.2 In continuous mode, render all visible pages (viewport + 1 page buffer) into a composite bitmap
- [ ] 5.3 Set scroll range to total composite height, paint composite at scroll offset
- [ ] 5.4 Rebuild composite on scroll or resize, only re-render pages that intersect the viewport
