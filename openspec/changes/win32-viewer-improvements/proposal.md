## Why

Both the Win32 viewer (Windows) and Qt viewer (Linux) share the same problems: only a single static page renders, there is no navigation between pages, scaling distorts content on resize, and text is blurry due to missing DPI awareness. Users cannot browse multi-page documents or switch between reading modes, making the plugin impractical on either platform.

## What Changes

- Add page navigation (next/prev/first/last) via keyboard and scroll — both viewers
- Fix page rendering to scale correctly on window resize (proper zoom calculation) — both viewers
- Add DPI-aware rendering so text stays sharp — engine layer (shared)
- Add paged vs. continuous scrolling mode toggle — both viewers
- Ensure the rendered page fills the available viewport without distortion — both viewers
- Viewer logic (navigation state, zoom math, mode switching) lives in a shared module; platform code is limited to HWND/Qt widget plumbing

## Capabilities

### New Capabilities
- `viewer-navigation`: Page navigation controls and keyboard shortcuts for stepping through multi-page documents
- `viewer-rendering`: Correct scaling, DPI-aware rendering, and distortion-free resize behavior
- `viewer-modes`: Paged (one page at a time) vs. continuous (scroll through all pages) reading modes

### Modified Capabilities

_(none — no existing specs)_

## Impact

- `src/viewer_win32.h` / `src/viewer_win32.cpp` — Win32 viewer gains navigation, zoom fix, modes
- `src/viewer.h` / `src/viewer.cpp` — Qt viewer gains same features, sharing logic where possible
- `src/mupdfengine.cpp` / `src/djvuengine.cpp` — DPI-aware render support (shared engines)
- `src/document.h` — `renderPage()` may gain DPI parameter (shared interface)
