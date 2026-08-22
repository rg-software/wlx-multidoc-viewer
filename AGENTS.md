# AGENTS.md

## Project Overview

WLX multi-document viewer plugin for Total Commander and Double Commander. Displays PDF, DjVu, EPUB, XPS, comic archives, images, and more inside the lister panel.

## Build

Windows (VS 2022) uses a standalone vcpkg at `C:\vcpkg`. Linux uses system packages via `find_library` (see CMakeLists.txt) — no vcpkg needed.

```bash
# Windows: configure + build (must run from vcvarsall shell)
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 && cmake --preset windows-x64-release && cmake --build --preset windows-release'

# Linux: configure + build
cmake --preset linux-release && cmake --build --preset linux-release
```

Output: `build/release/Release/wlx-multidoc-viewer.wlx64` (Windows), `build/linux-release/wlx-multidoc-viewer.wlx64` (Linux).

## Architecture

```
src/
  plugin.cpp            WLX entry points (ListLoad, ListCloseWindow, etc.)
  wlxplugin.h           WLX API types and DCPCALL macro
  document.h            DocumentEngine interface (open/render/text/outline)
  formatdispatcher.cpp  Routes file extensions to the right engine
  mupdfengine.*         MuPDF backend (PDF, XPS, EPUB, images, HTML)
  djvuengine.*          DjVuLibre backend (DJVU, DJV)
  viewer_win32.*        Win32 viewer (Windows) — pure HWND, per-page BitBlt paint
  viewer.*              Qt viewer (Linux) — QFrame, QScrollArea, ViewerCanvas widget
```

### Platform split

- **Windows**: Pure Win32 viewer. No Qt widgets. Qt6 Core+Gui statically linked for QImage/QString only. No Qt DLLs at runtime.
- **Linux**: Qt6 Widgets viewer. Full Qt event loop.
- `plugin.cpp` uses `#ifdef _WIN32` to select the path. Shared logic should live in platform-agnostic headers.

### Engines

MuPDF and DjVuLibre are linked as static libraries on Windows via vcpkg and as system libraries on Linux. Both return `QImage` from `renderPage(page, zoom)`. The engine interface (`DocumentEngine`) is platform-agnostic.

### Build system

- `CMakePresets.json`: VS 2022 generator + `x64-windows-static-md` triplet (Windows); Ninja (Linux)
- `vcpkg.json`: manifest mode with builtin-baseline (Windows only)
- `overlay-ports/djvulibre/`: custom port with manual config.cmake (debug+release imported locations, empty API macros for static linking)
- `CMakeLists.txt`: platform-conditional — `Qt6::Core`+`Qt6::Gui` on Windows, `Qt6::Widgets` on Linux; `find_package` on Windows vs `find_library` on Linux

## Conventions

- C++17, no exceptions in engine code (fz_try/fz_catch)
- WLX API uses `DCPCALL` for exports, `HANDLE` for opaque window pointers
- Detect string must fit in 260 chars (WLX buffer limit)
- No toolbar — TC provides its own navigation UI
- `ListLoad` returns an HWND (Windows) or widget pointer (Linux)

## Known Issues

### Fixed (in viewer-baseline)
- ~~Distortion on resize~~ — fixed with `setWidgetResizable(false)` and viewport-anchored relayout
- ~~No paged/continuous mode toggle~~ — V key toggles, Shift+V cycles fit mode
- ~~DjVu RGB/BGR swap and vertical flip~~ — fixed: `DDJVU_FORMAT_RGB24` + removed redundant `flipped(Qt::Vertical)`
- ~~DPI awareness hardcoded~~ — fixed: viewers pass system DPI scale (`GetDpiForWindow` / Qt `devicePixelRatioF`) into `ViewerController::setDpiScale()`; fit math and strip geometry are DPI-aware
- ~~Strip height cap (1.5M px) truncates scroll range~~ — fixed: continuous mode uses a per-page virtual canvas (`m_pageRects`/`m_contentSize`); the scrollbar covers the full document and memory is bounded by the per-page render cache (`kCacheWindowPages` instead of a single tall bitmap)
- ~~Mixed page sizes misalign in continuous strip~~ — fixed: each page is laid out with its own scaled dimensions and centered; uniform stride removed
- ~~Fit-to-page overflow (~16px clip)~~ — fixed: fit zoom and continuous paint now use the same page-area size (info panel height subtracted, no margin inset)

### Open gaps

| Gap | Severity | Note |
|---|---|---|
| **Windows `G` key go-to-page** | Low | Spec requires dialog; no handler in `viewer_win32.cpp`. Qt has `QInputDialog`. |
| **Asynchronous rendering** | Medium | Continuous mode renders newly-visible pages synchronously on the UI thread (one-frame stall on fast jumps). The per-page cache helps; a background worker is planned in `async-render-worker`. |
| **Qt build untested** | ~~Medium~~ Resolved | Linux build verified (`cmake --preset linux-release`); offscreen smoke test confirmed paged + continuous rendering (was all-gray: `ViewerCanvas` controller was never wired via `setController`). In-host DC verification still pending. |
