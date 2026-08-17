# AGENTS.md

## Project Overview

WLX multi-document viewer plugin for Total Commander and Double Commander. Displays PDF, DjVu, EPUB, XPS, comic archives, images, and more inside the lister panel.

## Build

Standalone vcpkg at `C:\vcpkg`. VS 2022 at `C:\Program Files\Microsoft Visual Studio\2022\Community`.

```bash
# Configure + build (must run from vcvarsall shell)
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 && cmake --preset windows-x64-release && cmake --build --preset windows-release'
```

Output: `build/release/Release/wlx-multidoc-viewer.dll`

## Architecture

```
src/
  plugin.cpp            WLX entry points (ListLoad, ListCloseWindow, etc.)
  wlxplugin.h           WLX API types and DCPCALL macro
  document.h            DocumentEngine interface (open/render/text/outline)
  formatdispatcher.cpp  Routes file extensions to the right engine
  mupdfengine.*         MuPDF backend (PDF, XPS, EPUB, images, HTML)
  djvuengine.*          DjVuLibre backend (DJVU, DJV)
  viewer_win32.*        Win32 viewer (Windows) — pure HWND, BitBlt paint
  viewer.*              Qt viewer (Linux) — QFrame, QScrollArea, QLabel
```

### Platform split

- **Windows**: Pure Win32 viewer. No Qt widgets. Qt6 Core+Gui statically linked for QImage/QString only. No Qt DLLs at runtime.
- **Linux**: Qt6 Widgets viewer. Full Qt event loop.
- `plugin.cpp` uses `#ifdef _WIN32` to select the path. Shared logic should live in platform-agnostic headers.

### Engines

MuPDF and DjVuLibre are linked as static libraries via vcpkg. Both return `QImage` from `renderPage(page, zoom)`. The engine interface (`DocumentEngine`) is platform-agnostic.

### Build system

- `CMakePresets.json`: VS 2022 generator, `x64-windows-static-md` triplet
- `vcpkg.json`: manifest mode with builtin-baseline
- `overlay-ports/djvulibre/`: custom port with manual config.cmake (debug+release imported locations, empty API macros for static linking)
- `CMakeLists.txt`: platform-conditional — `Qt6::Core`+`Qt6::Gui` on Windows, `Qt6::Widgets` on Linux

## Conventions

- C++17, no exceptions in engine code (fz_try/fz_catch)
- WLX API uses `DCPCALL` for exports, `HANDLE` for opaque window pointers
- Detect string must fit in 260 chars (WLX buffer limit)
- No toolbar — TC provides its own navigation UI
- `ListLoad` returns an HWND (Windows) or widget pointer (Linux)

## Known Issues

See `openspec/changes/win32-viewer-improvements/` for planned improvements:
- No page navigation (single page only)
- Distortion on resize
- Blurry text (missing DPI awareness)
- No paged/continuous mode toggle
