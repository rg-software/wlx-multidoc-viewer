# AGENTS.md

## Project Overview

WLX multi-document viewer plugin for Total Commander and Double Commander. Displays PDF, DjVu, EPUB, XPS, comic archives, images, and CHM inside the lister panel.

## Build

Windows uses vcpkg (any checkout location exposed as the `VCPKG_ROOT` environment variable; manifest mode, `x64-windows-static-md` triplet). Linux uses system packages — MuPDF is found via `pkg-config --static` (Debian/Ubuntu ship only static `libmupdf.a` + `libmupdf-third.a`) falling back to `find_library` for a shared `.so` (CI builds one via vcpkg) — see CMakeLists.txt.

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
  document.h            DocumentEngine interface (open/render/text/outline + pageText/PageText + pageLinks/LinkItem + animation virtuals)
  formatdispatcher.cpp  Routes file extensions to the right engine
  mupdfengine.*         MuPDF backend (PDF, XPS, EPUB, images, HTML); pageText from fz_stext; searchText from fz_search_page_cb; pageLinks from fz_load_links/fz_resolve_link_dest
  djvuengine.*          DjVuLibre backend (DJVU, DJV); pageText/search unavailable (see AGENTS gaps)
  chmengine.*           CHM backend via libchm (archive access) + MuPDF HTML pipeline (render/text/search); reading order + nested .hhc outline; topic links resolved to archive pages (relative hrefs + #fragment anchors)
  comicengine.*         Comic archive backend via libarchive (CBR/CB7); Qt image decode, natural page order
  fb2zipengine.*        FB2-in-zip backend (.zip registered in the dispatcher but declined unless an entry sniffs as FictionBook, so other WLX plugins still get their turn); libarchive probe/extract (64 MB cap, natural-sorted first match) -> MuPdfEngine::openBuffer with a .fb2 magic
  imageengine.*         Standalone raster engine (jpg/png/gif/bmp/ico) via QImageReader; one page per file, in-place GIF animation
  imagefolder.*         Sibling raster discovery/ordering in a directory (natural order, shared with comics)
  naturalsort.h/cpp     Shared natural (numeric-aware) filename comparator used by comic + image engines
  viewercontroller.*    Shared state + commands + virtual-canvas layout + render cache + selection + text search state
  favorites.*           Per-document page-favorites store (singleton, JSON, QSaveFile atomic writes, debounced, change notification)
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

**MuPDF version floor (Linux):** the system `libmupdf-dev` must be ≥ 1.23 to compile (`FZ_STEXT_ACCURATE_BBOXES` from 1.25+, `fz_style_document` from 1.28+, richer `fz_error` codes `FZ_ERROR_FORMAT`/`FZ_ERROR_SYSTEM` from 1.24+). `CMakeLists.txt` runs three `check_c_source_compiles` capability probes against the found lib and feeds the results to all engine consumers via `unofficial::libmupdf::libmupdf`'s `INTERFACE_COMPILE_DEFINITIONS`: `MUPDF_HAVE_STEXT_ACCURATE_BBOXES` (both engines build stext via the shared `newStextPage()` helper, which zeroes flags without it), `MUPDF_HAVE_STYLE_DOCUMENT` (mupdfengine falls back to the context-level `fz_set_user_css`, the same hook chmengine always uses), and `MUPDF_HAVE_FZ_ERROR_FORMAT` (mupdfengine maps `kErrorFormat`/`kErrorSystem` to the legacy `SYNTAX`/`MEMORY` codes). The Windows overlay pins 1.28.x and hard-codes all three ON. Always use the probes (or the version macros) — never reference the raw symbols without a guard.

Standalone raster images (`.jpg/.jpeg/.png/.gif/.bmp/.ico`) route to `ImageEngine` (Qt `QImageReader`). Every listed raster is a single page; multi-frame GIFs animate in place and are **not** step-through pages. GIF decoding has a Qt gotcha: `QGifHandler` does **not** implement `jumpToImage`/`jumpToNextImage`, so `ImageEngine` decodes frames 0..N sequentially via `read()` from a fresh reader (`readFrameImage`). Sibling images in the same directory (natural order via `naturalsort.h`) are opened at next/prev document bounds. TIFF (`.tif/.tiff`) is a **document**, not an image: it routes to the default MuPDF engine, where each TIFF IFD becomes a page (real multi-page TIFF support), and it is excluded from the image-folder sibling set — matching how PDF/CBR/CB7 are handled. WEBP (`.webp`) currently has **no decoder** (neither Qt `QImageReader` nor MuPDF ships one in this trimmed build) and is not routed; see the AGENTS gaps table.

### Build system

- `CMakePresets.json`: VS 2026 generator + `x64-windows-static-md` triplet (Windows); Ninja (Linux)
- `vcpkg.json`: manifest mode with builtin-baseline (Windows only)
- `overlay-ports/djvulibre/`: custom port with manual config.cmake (debug+release imported locations, empty API macros for static linking)
- `CMakeLists.txt`: platform-conditional — `Qt6::Core`+`Qt6::Gui` on Windows, `Qt6::Widgets`+`Qt6::PrintSupport` on Linux; `find_package` on Windows vs `pkg-config --static` (distro `libmupdf.a`) with a `find_library` shared fallback on Linux. CHMLib and LibArchive ship no clean imported-target configs, so both are located via `find_path`/`find_library` into hand-rolled imported targets (LibArchive also carries its codec backends lz4/lzma/zstd/bz2/openssl + Windows system libs as per-config INTERFACE deps).

## Conventions

- C++17, no exceptions in engine code (fz_try/fz_catch)
- WLX API uses `DCPCALL` for exports, `HANDLE` for opaque window pointers
- Detect string must fit in 260 chars (WLX buffer limit)
- The viewer hosts its own toolbar: one shared `ToolbarPresenter`/`SidebarPresenter` pair drives thin per-platform backends (`toolbar_win32.*`/`toolbar_qt.*`, `sidebar_win32.*`/`sidebar_qt.*`). State flows controller -> presenter -> backend and backend events -> presenter -> controller, so keyboard and toolbar never diverge.
- The toolbar copy button copies the current selection (`ToolbarPresenter::onCopy`); it is enabled only while a selection exists.
- Hyperlinks flow through the engine-level `pageLinks()`/`LinkItem` model (page-space hot zones + internal page/anchor or external URI). The controller caches them and owns hit-testing (`linkAt`) and activation (`followLink`); each platform viewer only supplies the pointing-hand cursor, the Ctrl+click modifier check, and an external-URL launcher installed via `setExternalLinkHandler` (Win32 `ShellExecuteW`, Qt `QDesktopServices::openUrl`). Ctrl+click follows a link; a plain press still selects/pans. The pointing-hand cursor appears over a link only while Ctrl is held and updates on the Ctrl key transition (no pointer move needed), so it never implies that a plain click navigates.
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
- **MOBI cover page is a synthetic engine-side page, not an HTML injection** — `MuPdfEngine::tryOpenMobi` reads the EXTH cover record (type 201) and stores the decoded `QImage`; `renderPage(1)` draws it scaled to the body text area (page minus `@page{margin:3em 2em}`), and all body pages shift +1 in the public API. The cover is **deliberately NOT injected into `index.html`**: adding any cover element to a reflowable htdoc makes MuPDF's restartable layout drop the following `mbp:pagebreak` `page-break-before` elements, collapsing section breaks (Contents/for-Sharon/Chapter boundaries). Keep the body byte-identical and never reintroduce HTML injection (see change `add-mobi-cover-page`). This also removes the `fz_extract_html_from_mobi` internal-symbol dependency from the engine (risk R3 for a hidden Linux symbol is gone).

### Fixed (in add-image-browsing-and-animation)
- ~~Standalone raster images have no animation and no folder navigation~~ — a new `ImageEngine` (`QImageReader`) now handles `.jpg/.jpeg/.png/.gif/.tif/.tiff/.bmp/.webp` as single-page documents, plays multi-frame GIFs in place (per-frame delay + loop count via the `DocumentEngine` animation virtuals `isAnimated`/`frameDelayMs`/`advanceFrame`), and the `ViewerController` opens sibling images at next/prev document bounds (`ImageFolder` + shared `naturalsort`). The `QGifHandler` jumpToImage gotcha and the Win32 playback timer (delivered as `WM_TIMER`, not a custom message) are both accounted for; sample generators live in `tools/generate_sample_images.py` and `examples/` ships generated `sample1.jpg`…`sample5.tiff` + `sample-animated.gif`.

### Fixed (in add-icc-color-management)
- ~~CMYK/ICC images render near-black (naive CMYK→RGB fallback)~~ — the `trim-binary-size` drop of lcms2 (`FZ_ENABLE_ICC=0` in the libmupdf overlay port) made MuPDF convert embedded CMYK JPEGs with a naive `1-min(1, c+k)` formula, collapsing ICC-managed content (e.g. the title page of `examples/AC3_GW_Notebook_GER.pdf`) to near-black/dark-red with the pattern invisible. Fix: re-enabled ICC in the overlay port (`FZ_ENABLE_ICC=1` in `overlay-ports/libmupdf/CMakeLists.txt`), added the `lcms` vcpkg dependency, linked `lcms2::lcms2`, and added `find_dependency(lcms2 CONFIG)` to `unofficial-libmupdf-config.cmake.in` (without it the consuming `find_package(unofficial-libmupdf)` fails on the unresolved transitive target). Windows-only (Linux ships distro MuPDF with ICC); Windows x64 binary grows ~22 → 23.78 MB. Regression harness: `tests/harness_icc.cpp` (renders the title page headlessly and asserts brown pixels / no red-collapse signature).

### Fixed (in fix-refit-fit-zoom-on-navigation)
- ~~Two-tap PgUp/PgDn on mixed-size documents in paged mode (page 1, 2, 2, 3, 3, …)~~ — the fit zoom was computed once against the unit that was current at open/relayout time and frozen across navigation, so a non-anchor page whose aspect was taller than that unit overflowed the viewport and PgDn scrolled *within* it on the first press. Fix: paged mode now re-fits the active fit mode to the current view unit on every navigation (`ViewerController::refitAfterNavigation`), fit-to-width in paged mode targets the current unit's width (continuous mode keeps the doc-wide widest row), and PgUp/PgDn treat a unit whose overflow is within the 32 px block-overlap band as fully visible (`unitRequiresVerticalScroll`). Manual zoom never refits; continuous mode is untouched. Uniform documents see no relayout (zoom-epsilon no-op). Regression harness: `tests/harness_refit.cpp`.

### Fixed (in add-hyperlink-navigation)
- ~~Hyperlinks in linked documents (PDF/EPUB/XPS/MOBI/CHM) were not navigable: link text rendered but Ctrl+click did nothing and external URLs were unreachable.~~ — added the engine-level `pageLinks()`/`LinkItem` model (`src/document.h`). `MuPdfEngine::pageLinks` extracts hot zones with `fz_load_links` and resolves internal targets with `fz_resolve_link_dest`/`fz_page_number_from_location` (applying the synthetic MOBI cover shift and reading the `XYZ` anchor y); external URIs are classified with `fz_is_external_link`. `ChmEngine::pageLinks` reuses the topic's MuPDF HTML links and resolves relative hrefs (with `./`, `../`, leading `/`, percent-decoding, backslashes) plus `#fragment` anchors against the archive page list via the existing `normalizePath`/`pageIndexOf`. `ViewerController` caches links per page, hit-tests with `linkAt` (via `pageTransform`, so zoom/rotation/continuous all work), and follows them with `followLink` (page jump + clamped anchor scroll, or the injected external launcher). Viewers show a pointing-hand cursor over a link only while Ctrl is held (refreshed on the Ctrl key transition) and follow on Ctrl+click so plain press still selects/pans. Regression harness: `tests/harness_links.cpp` with fixtures `examples/sample-links.pdf` / `examples/sample-links.chm` (generated by `tools/generate_sample_links.py`).

### Fixed (in add-fb2zip-support)
- ~~FB2 books packed inside a zip were not openable~~ — a new `Fb2ZipEngine` (registered for the `zip` suffix in the dispatcher) probes the archive with libarchive and, if any `.fb2` entry sniffs as FictionBook (first 4096 bytes contain `<FictionBook`, mirroring MuPDF's fb2 recognizer, natural-sorted first match, 64 MB entry-size cap), extracts it and feeds `MuPdfEngine::openBuffer` with a `.fb2` magic so the FB2 handler and duotone FB2 theming apply. The zip is **declined** otherwise (`open()` returns false → `ListLoad` returns 0 → the host gives the next WLX plugin its turn): non-archives, zips with no `.fb2` entry, `.fb2`-named entries that are not FictionBook, over-cap entries, decryption/read errors, and delegated MuPDF failures. It deliberately does **not** fall through to `MuPdfEngine` — MuPDF's own CBZ handler also claims `zip` and would silently open every zip. `open()` was refactored into `makeContext`/`finishOpen` (shared by file and buffer paths; `finishOpen` derives the reflowable-suffix theming split from the magic name). Regression harness `tests/harness_fb2zip.cpp` (routed through `createEngine`, skip-contract + multi-entry natural-order checks); fixtures `examples/{sample,not-fb2,fake-fb2,multi}.fb2.zip` generated by `tools/generate_sample_fb2zip.py`.

### Open gaps

| Gap | Severity | Note |
|---|---|---|
| **Synchronous first-render of a new page** | Low | Rendering happens on the UI thread only the first time a page enters the viewport; the per-page LRU cache (`kCacheWindowPages`) makes revisits instant. A background render worker was tried and reverted — thread-safety + FIFO-order complexity did not justify the latency gain for a single lister (see `async-render-worker` change, abandoned). |
| **DjVu text-layer selection & search** | Low | The vcpkg djvulibre static build does not export the core miniexp accessors (`miniexp_car/cdr/consp/symbolp/to_int`), so `ddjvu_document_get_pagetext` trees cannot be walked. DjVu pages report no text layer and `supportsSearch() == false` (selection and find work only for MuPDF-backed formats). Revisit if a djvulibre build with the miniexp public API is available, or add a local miniexp.h overlay. |
| **Qt in-host verification** | Medium | The toolbar/sidebar/print rework changed `viewer.*`, `toolbar_qt.*`, `sidebar_qt.*`, `print_qt.cpp`; Linux must re-verify `cmake --preset linux-release` (new `Qt6::PrintSupport` dependency) and scroll + selection + toolbar behavior in Total/Double Commander. Windows interactive smoke tests (task 4.4) are likewise pending. The add-hyperlink-navigation change adds a Qt cursor/`KeyPress` branch in `viewer.cpp` (modifier-gated link hand + Ctrl+click) that likewise needs the Linux build and smoke test. |
| **Print worker on Qt** | Low | QPrinter must be used on the main thread, so the Qt print path renders synchronously instead of on a `PrintCoordinator` worker; the Win32 path uses the worker. Page/copy resolution and fit math are still shared. |
| **CHM engine Linux build** | Low | `chmengine.*` now compiles and links in `cmake --preset linux-release` (verified in-host against MuPDF 1.28 and against the 1.23 Ubuntu headers via the capability probes). Still pending: interactive verification of CHM rendering + Qt sidebar ESC-forwarding parity in a file manager on Linux. |
| **Comic engine real-RAR verification** | Low | `ComicEngine` is verified against a genuine RAR5 CBR (`examples/sample.cbr`) in the host; 7-Zip-based CB7 still awaits a real sample (libarchive handles the format). |
| **WEBP decode support** | Medium | WEBP has no decoder in the trimmed build: Qt's `QImageReader` (no webp image-format plugin vendored in `qtbase`) and MuPDF (no `load-webp`) both fail on `.webp`, so it is not routed anywhere. Treating webp as an image (like GIF, incl. animated webp) means adding the Qt webp plugin + `libwebp` to the `qtbase` overlay port and rebuilding it — a build-system change, not a flag flip. |
