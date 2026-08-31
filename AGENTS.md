# AGENTS.md

## Project Overview

WLX multi-document viewer plugin for Total Commander and Double Commander. Displays PDF, DjVu, EPUB, XPS, comic archives, images, and CHM inside the lister panel.

## Build

Windows uses vcpkg (any checkout location exposed as the `VCPKG_ROOT` environment variable; manifest mode, `x64-windows-static-md` triplet). Linux uses system packages via `find_library` (see CMakeLists.txt) — no vcpkg needed.

```bash
# Windows: configure + build from a VS 2026 developer shell
cmake --preset windows-x64-release && cmake --build --preset windows-release

# Linux: configure + build
cmake --preset linux-release && cmake --build --preset linux-release
```

Output: `build/release/Release/MultidocViewer.wlx64` (Windows), `build/linux-release/MultidocViewer.wlx64` (Linux).

Release sizes (after the `trim-binary-size` change that drops MuPDF's embedded CJK/Noto fonts and trims Qt features): Windows x64 `MultidocViewer.wlx64` ~22 MB, Windows x86 `MultidocViewer.wlx` ~19 MB. Fonts embedded in the trimmed `overlay-ports/libmupdf` are limited to Base-14 (`urw`) + SIL (`sil`); CJK/emoji fall back to system fonts (DirectWrite on Windows, fontconfig on Linux). Rebuild the `libmupdf` vcpkg port (which requires the VS developer env + `VCPKG_ROOT`) after touching the overlay.

## Architecture

```
src/
  plugin.cpp            WLX entry points (ListLoad, ListCloseWindow, etc.)
  wlxplugin.h           WLX API types and DCPCALL macro
  document.h            DocumentEngine interface (open/render/text/outline + pageText/PageText)
  formatdispatcher.cpp  Routes file extensions to the right engine
  mupdfengine.*         MuPDF backend (PDF, XPS, EPUB, images, HTML); pageText from fz_stext; searchText from fz_search_page_cb
  djvuengine.*          DjVuLibre backend (DJVU, DJV); pageText/search unavailable (see AGENTS gaps)
  chmengine.*           CHM backend via libchm (archive access) + MuPDF HTML pipeline (render/text/search); reading order + nested .hhc outline
  comicengine.*         Comic archive backend via libarchive (CBR/CB7); Qt image decode, natural page order
  viewercontroller.*    Shared state + commands + virtual-canvas layout + render cache + selection + text search state
  textselection.*       Platform-agnostic text-selection model (anchor/focus, ranges)
  searchcontroller.*    Whole-document search worker thread (progressive per-page results, atomic cancel)
  toolbar.*             Shared toolbar interface + ToolbarPresenter (controller state <-> backend)
  toolbar_icons.*       Programmatic monochrome toolbar glyphs (QImage), no binary assets
  toolbar_win32.*       Win32 toolbar backend (child HWND + owner-drawn BUTTON/EDIT/STATIC)
  toolbar_qt.*          Qt toolbar backend (QToolButtons/QLineEdit/QLabel row)
  sidebar.*             Shared outline sidebar presenter/backend contract (flat ids, lazy children)
  sidebar_win32.*       Win32 sidebar backend (WC_TREEVIEW, per-level lazy population)
  sidebar_qt.*          Qt sidebar backend (QTreeWidget dock area, lazy population)
  printcoordinator.*    Shared print job pipeline (pages × copies, printer-resolution render worker)
  print_win32.cpp       PrintDlgEx -> printer DC -> StretchDIBits spool
  print_qt.cpp          QPrintDialog + QPrinter painting (Linux only)
  textselection.*       Platform-agnostic text-selection model (anchor/focus, ranges)
  viewer_win32.*        Win32 viewer (Windows) — pure HWND, per-page BitBlt paint, hosts toolbar/sidebar
  viewer.*              Qt viewer (Linux) — QFrame + ViewerCanvas widget, hosts toolbar/sidebar
```

### Platform split

- **Windows**: Pure Win32 viewer. No Qt widgets. Qt6 Core+Gui statically linked for QImage/QString only. No Qt DLLs at runtime.
- **Linux**: Qt6 Widgets viewer. Full Qt event loop.
- `plugin.cpp` uses `#ifdef _WIN32` to select the path. Shared logic should live in platform-agnostic headers.

### Engines

MuPDF and DjVuLibre are linked as static libraries on Windows via vcpkg and as system libraries on Linux. Both return `QImage` from `renderPage(page, zoom)`. The engine interface (`DocumentEngine`) is platform-agnostic.

### Build system

- `CMakePresets.json`: VS 2026 generator + `x64-windows-static-md` triplet (Windows); Ninja (Linux)
- `vcpkg.json`: manifest mode with builtin-baseline (Windows only)
- `overlay-ports/djvulibre/`: custom port with manual config.cmake (debug+release imported locations, empty API macros for static linking)
- `CMakeLists.txt`: platform-conditional — `Qt6::Core`+`Qt6::Gui` on Windows, `Qt6::Widgets`+`Qt6::PrintSupport` on Linux; `find_package` on Windows vs `find_library` on Linux. CHMLib and LibArchive ship no clean imported-target configs, so both are located via `find_path`/`find_library` into hand-rolled imported targets (LibArchive also carries its codec backends lz4/lzma/zstd/bz2/openssl + Windows system libs as per-config INTERFACE deps).

## Conventions

- C++17, no exceptions in engine code (fz_try/fz_catch)
- WLX API uses `DCPCALL` for exports, `HANDLE` for opaque window pointers
- Detect string must fit in 260 chars (WLX buffer limit)
- The viewer hosts its own toolbar: one shared `ToolbarPresenter`/`SidebarPresenter` pair drives thin per-platform backends (`toolbar_win32.*`/`toolbar_qt.*`, `sidebar_win32.*`/`sidebar_qt.*`). State flows controller -> presenter -> backend and backend events -> presenter -> controller, so keyboard and toolbar never diverge.
- The toolbar copy button copies the current selection (`ToolbarPresenter::onCopy`); it is enabled only while a selection exists.
- `ListLoad` returns an HWND (Windows) or widget pointer (Linux)

### Git / Commit conventions

Write conventional, structured commit messages so the release pipeline can group them into categories (it parses `feat:`/`fix:` prefixes):

- **Format**: Conventional Commits — use a type prefix such as `feat:`, `fix:`, `docs:`, `refactor:`, `chore:`. Use a scoped prefix (e.g. `feat(comics):`, `fix(sidebar):`) when a subsystem is affected.
- **Mood**: imperative mood in the subject line (e.g. prefer "Add feature" over "Added feature" / "Adding feature").
- **Length**: keep the first line under 72 characters.
- **Issue Link**: use `Fixes #<id>` for bug fixes, `Refs #<id>` otherwise. Skip the reference if the branch is not issue-based.

## Known Issues

### Fixed (in viewer-baseline)
- ~~Distortion on resize~~ — fixed with `setWidgetResizable(false)` and viewport-anchored relayout
- ~~No paged/continuous mode toggle~~ — V key toggles, Shift+V cycles fit mode
- ~~DjVu RGB/BGR swap and vertical flip~~ — fixed: `DDJVU_FORMAT_RGB24` + removed redundant `flipped(Qt::Vertical)`
- ~~DPI awareness hardcoded~~ — fixed: viewers pass system DPI scale (`GetDpiForWindow` / Qt `devicePixelRatioF`) into `ViewerController::setDpiScale()`; fit math and strip geometry are DPI-aware
- ~~Strip height cap (1.5M px) truncates scroll range~~ — fixed: continuous mode uses a per-page virtual canvas (`m_pageRects`/`m_contentSize`); the scrollbar covers the full document and memory is bounded by the per-page render cache (`kCacheWindowPages` instead of a single tall bitmap)
- ~~Mixed page sizes misalign in continuous strip~~ — fixed: each page is laid out with its own scaled dimensions and centered; uniform stride removed
- ~~Fit-to-page overflow (~16px clip)~~ — fixed: fit zoom and continuous paint now use the same page-area size (no margin inset)
- ~~Garbled text in embedded-CJK PDFs after the font trim~~ — fixed: the `trim-binary-size` change defined `NO_CJK` in the overlay port, but that macro also strips the builtin CJK **cmap tables** (`pdf-cmap-load.c` `#ifdef NO_CJK` → only Identity/TrueType cmaps), so documents whose glyph mapping needs e.g. `Adobe-Japan1-UCS2` rendered $\ne$ copy/search text correctly (verified byte-identical text extraction vs PyMuPDF 1.28.2 after the fix). Fix: use the font-only `TOFU_CJK`/`TOFU_CJK_EXT`/`TOFU_CJK_LANG` defines instead of `NO_CJK` — fonts stay trimmed, cmaps stay available.
- ~~MuPDF pinned at 1.26.10~~ — upgraded the overlay port to **1.28.3**, which required: vendoring the `thirdparty/mujs` files (`regexp.h`, `regexp.c`, `utf.h`, `utf.c`, `utfdata.h`) because 1.28.x tag tarballs ship that submodule empty yet `source/fitz/regexp.c`/`stext-search.c` include it; adding `FZ_ENABLE_MD=0` (Markdown/cmark-gfm dropped) and `FZ_ENABLE_HYPHEN=0` (avoids embedding the ~800KB hyph zips); dropping the now-shipped-in-tree `scripts/bin2coff.c` download.

### Open gaps

| Gap | Severity | Note |
|---|---|---|
| **Synchronous first-render of a new page** | Low | Rendering happens on the UI thread only the first time a page enters the viewport; the per-page LRU cache (`kCacheWindowPages`) makes revisits instant. A background render worker was tried and reverted — thread-safety + FIFO-order complexity did not justify the latency gain for a single lister (see `async-render-worker` change, abandoned). |
| **DjVu text-layer selection & search** | Low | The vcpkg djvulibre static build does not export the core miniexp accessors (`miniexp_car/cdr/consp/symbolp/to_int`), so `ddjvu_document_get_pagetext` trees cannot be walked. DjVu pages report no text layer and `supportsSearch() == false` (selection and find work only for MuPDF-backed formats). Revisit if a djvulibre build with the miniexp public API is available, or add a local miniexp.h overlay. |
| **Qt in-host verification** | Medium | The toolbar/sidebar/print rework changed `viewer.*`, `toolbar_qt.*`, `sidebar_qt.*`, `print_qt.cpp`; Linux must re-verify `cmake --preset linux-release` (new `Qt6::PrintSupport` dependency) and scroll + selection + toolbar behavior in Total/Double Commander. Windows interactive smoke tests (task 4.4) are likewise pending. |
| **Print worker on Qt** | Low | QPrinter must be used on the main thread, so the Qt print path renders synchronously instead of on a `PrintCoordinator` worker; the Win32 path uses the worker. Page/copy resolution and fit math are still shared. |
| **CHM engine Linux build** | Low | `chmengine.*` compiles against system libchm via `find_library`, but `cmake --preset linux-release` has not been re-run since the CHM engine landed (Windows-only verification so far). Also pending: Qt sidebar ESC-forwarding parity check on Linux. |
| **Comic engine real-RAR verification** | Low | `ComicEngine` is verified against a genuine RAR5 CBR (`examples/sample.cbr`) in the host; 7-Zip-based CB7 still awaits a real sample (libarchive handles the format). |
