## Why

The Win32 viewer for the WLX document plugin currently renders only a single static page with no navigation, distorted scaling when the window is resized, and blurry text due to missing DPI awareness. Users cannot browse multi-page documents or switch between paged and continuous reading modes, making the plugin impractical for real-world use.

## What Changes

- Add page navigation (next/prev/first/last) via keyboard and scroll
- Fix page rendering to scale correctly on window resize (proper zoom calculation)
- Add DPI-aware rendering so text stays sharp at any zoom level
- Add paged vs. continuous scrolling mode toggle
- Ensure the rendered page fills the available viewport without distortion

## Capabilities

### New Capabilities
- `win32-viewer-navigation`: Page navigation controls and keyboard shortcuts for stepping through multi-page documents
- `win32-viewer-rendering`: Correct scaling, DPI-aware rendering, and distortion-free resize behavior
- `win32-viewer-modes`: Paged (one page at a time) vs. continuous (scroll through all pages) reading modes

### Modified Capabilities

_(none — no existing specs)_

## Impact

- `src/viewer_win32.h` / `src/viewer_win32.cpp` — primary files to modify
- `src/mupdfengine.cpp` — may need DPI-aware render API or zoom matrix adjustments
- `src/document.h` — `renderPage()` signature may need DPI parameter
- Both engine implementations (MuPDF, DjVuLibre) — render output must support fractional zoom without quality loss
