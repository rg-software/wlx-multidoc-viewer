# add-cross-platform-toolbar — Design

## Context

Both viewers are thin shells over `ViewerController` (`src/viewercontroller.cpp`), which already owns navigation, zoom, fit modes, rotation, and paged/continuous state and notifies via `StateChangedCallback`. The Win32 viewer hosts one custom-painted child strip (`InfoPanelWin32`) inside its main HWND; the Qt viewer uses a fixed-height info bar widget. Engines return `QImage` from `renderPage` and expose per-page `extractText`, but no positional text search and no printing exist anywhere. See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**
- One shared toolbar definition; two native backends with behavioral parity
- Whole-document search with highlight overlay, driven by new engine API
- Printing through the platform print dialog, re-rendering at printer resolution
- DPI-aware sizing consistent with existing fit math

**Non-Goals:**
- Toolbar customization, movable/detachable toolbars, theming beyond system defaults
- Print preview, duplex/finishing controls beyond what the system dialog offers
- Regex or incremental-as-you-type search (search commits on Enter / button)
- Replacing TC's own lister chrome; bookmarks/annotations

## Decisions

### D1: Thin per-platform backends behind one shared interface
New files:
- `src/toolbar.h` — control-ID enum, abstract `ToolbarBackend` (create controls, set pressed/checked/disabled/text, set icons, query edit-box text), and `ToolbarPresenter` mapping controller state → backend calls and backend events → controller calls.
- `src/toolbar_win32.cpp` — child-HWND panel hosting individual common controls (`BUTTON`, `EDIT`, `STATIC`) created and laid out manually, following the `InfoPanelWin32` pattern.
- `src/toolbar_qt.cpp` — QWidget + QHBoxLayout of QToolButton/QLineEdit/QLabel.

Rationale: the user-requested wrapper approach keeps the Windows build free of Qt widgets. A plain panel with individual Win32 controls beats the `TOOLBAR` common control because the strip mixes buttons with edit boxes and labels; the toolbar control only handles buttons well. Alternative rejected: QToolBar on both platforms (violates the no-Qt-widgets Windows constraint).

### D2: Icons drawn programmatically
A small shared icon module renders monochrome glyphs (arrows, magnifier, rotate arcs, fit frames, printer) into HBITMAP/QPixmap at `size * dpiScale`. No binary resources, sharp at any DPI, single source for both backends. Raster assets can replace this later without spec impact.

### D3: Geometry
Add `viewer_settings::kToolbarBaseHeight`; effective height = base × dpiScale. Page-area layout subtracts toolbar (top) and info panel (bottom) in both viewers. Fit-mode cycling reuses `cycleFitMode()` (Manual → FitToPage → FitToWidth), so keyboard and toolbar share one path and stay synchronized via `StateChangedCallback`.

### D4: Engine search API (positional)
Extend `DocumentEngine`:

```cpp
struct TextMatch { int page; QVector<QRectF> rects; }; // rects normalized 0..1 page space
virtual bool supportsSearch() const;
virtual QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase);
```

Per-page contract; normalized rectangles make highlighting zoom/rotation-independent by construction. MuPDF implements it with `fz_search_page_cb` (quads → normalized; case-sensitive searches walk stext characters because MuPDF's built-in search is always case-insensitive); DjVuLibre reports no capability (`supportsSearch() == false`) — the static djvulibre build does not export the miniexp accessors needed to walk `ddjvu_document_get_pagetext` trees (see AGENTS.md open gaps), so the word-box search is not linkable and find controls stay disabled for DjVu. Engines lacking positional text report `supportsSearch() == false`.

### D5: Search orchestration on a worker thread
New `src/searchcontroller.*`: owns a `std::thread` walking pages from the current page (wrapping), calling `engine->searchText`, delivering results via callbacks. Thread-safety: each engine serializes its MuPDF/DjVu calls behind a per-engine mutex (rendering may pause momentarily while a page is searched). Cancellation via atomic flag checked per page; a new search or cleared term resets. Alternative considered (separate second engine instance per document for search) rejected as doubling memory for little gain at plugin scale.

UI marshaling is platform-owned: Win32 viewer posts a custom message to its main HWND; Qt viewer uses queued invocation. The controller layer stays thread-agnostic.

### D6: Highlight overlay painted in shared code
`ViewerController::renderVisiblePages` gains an optional match-overlay step: after composing the page image, translucent fills are painted with QPainter using normalized rects scaled to the rendered image (active match gets distinct fill + outline). This works identically in Core+Gui-only Windows builds and full Widgets Linux builds, and automatically survives zoom/rotation because coordinates are recomputed per render. Search result state (match list, active index) lives in ViewerController so rendering and `trackCurrentPage` see one truth.

### D7: Printing via system dialog
Shared `PrintCoordinator` receives a resolved job — opaque settings blob plus concrete page/copy list — and loops render → spool. Backends:
- **Windows** `print_win32.cpp`: `PrintDlgEx` (gives page range/copies UI for free) → DEVMODE → printer DC via `CreateDCW`; pages re-rendered with zoom derived from `GetDeviceCaps(HORZRES/VERTRES)` at printer DPI and blitted with `StretchDIBits` from the QImage bits; rotation applied before scaling. Uses winspool/gdi32 — no new dependencies.
- **Linux** `print_qt.cpp`: `QPrintDialog` + QPrinter from `Qt6::PrintSupport` (new link dependency, Linux only); paint rendered images via QPainter onto the printer painter.

Spooling runs on a worker thread with progress callbacks marshaled like search results; cancelling the dialog aborts before any job starts. Rendering at printer DPI (not screen-bitmap upscaling) satisfies the sharpness requirement.

### D8: Keyboard focus neutrality (both platforms)
Central rule: viewer shortcut handling runs only when focus is outside toolbar edit boxes. Win32: the main wndProc checks `GetFocus()` against the page/edit controls before acting on keys; edit boxes get normal tab/focus behavior. Qt: line edits consume keys naturally; shortcuts bound to the page area's focus context. Existing hotkeys are otherwise unchanged.

### D9: Copy button ships as a disabled placeholder
The copy control is created through the normal toolbar backends but is permanently disabled: text selection (mouse-drag highlight, engine hit-testing, selection model) is owned by a separate future change. No fallback behavior is wired (rejected alternative: copying the current page's full text — misleading semantics for a "copy selection" affordance). The button exists so the later change only has to flip enablement and bind one handler.

### D10: Outline sidebar
Data comes from the existing `DocumentEngine::outline()` (`OutlineItem` tree) — no engine changes needed. New shared `src/sidebar.h` + `SidebarPresenter`: flattening/expansion state, active-entry tracking, and backend events → `goToPage`. Backends:
- **Windows** `sidebar_win32.cpp`: child panel hosting `WC_TREEVIEW` with owner-drawn DPI-scaled icons; populated lazily per expansion to keep huge TOCs cheap.
- **Linux** `sidebar_qt.cpp`: `QTreeWidget` in a fixed-width dock-area widget.

Geometry: page-area layout subtracts sidebar width (base × dpiScale, only while visible) in addition to toolbar and info panel; the toolbar gains a sidebar-toggle button that appears only when `outline()` is non-empty. Active entry follows `trackCurrentPage`/`pageAtScrollOffset`, so keyboard and scroll navigation highlight the matching TOC entry identically to sidebar clicks. When the outline is empty the sidebar is never created rather than shown empty.

## Platform-specific code

| Concern | Windows | Linux |
|---|---|---|
| Toolbar backend | `toolbar_win32.cpp`: child HWND + BUTTON/EDIT/STATIC, WM_CTLCOLOR*, manual layout, owner-drawn icons | `toolbar_qt.cpp`: QWidget + QLayout, QToolButton checkable states |
| Outline sidebar | `sidebar_win32.cpp`: WC_TREEVIEW child panel, lazy population | `sidebar_qt.cpp`: QTreeWidget dock-area widget |
| Icon rasterization | QPainter→QImage→HBITMAP (DIB section) | QPixmap directly |
| Key routing | wndProc focus checks | QLineEdit consumes; QShortcut scoping |
| Async marshal | PostMessage custom msg | Queued signal/invokeMethod |
| Print | PrintDlgEx + GDI StretchDIBits (existing libs) | Qt6::PrintSupport (new dep, Linux-only CMake target) |
| DPI | dpiScale from GetDpiForWindow, relayout on WM_DPICHANGED | devicePixelRatioF |

Shared: ToolbarPresenter, SidebarPresenter, icon glyphs, SearchController, highlight painting, PrintCoordinator job loop, all ViewerController changes.

## Risks / Trade-offs

- [MuPDF/DjVu not thread-safe] → per-engine mutex around every engine call; search yields between pages so UI stalls stay sub-frame
- [DjVu hidden text absent on some pages] → empty result treated as "no matches", never an error; find stays enabled if any page has text
- [Qt6::PrintSupport grows Linux build] → Linux-only link; Windows path adds nothing
- [TC lister forwards keys unexpectedly] → focus-neutrality checks centralized in one place per platform, covered by dedicated test task
- [Print margins clipping] → use physical offsets (PHYSICALOFFSETX/Y on Windows; QPrinter pageRect on Linux), fit-to-printable-area math shared
- [Worker callbacks outliving document close] → SearchController/print jobs cancelled and joined synchronously in `closeDocument` before engine teardown
- [Very large/deep outlines (thousands of nodes)] → lazy per-level population in both backends; active-entry search walks only the expanded path
- [Sidebar tree steals keyboard focus from page area] → same central focus-neutrality rule as D8 applied to the sidebar backends

## Migration Plan

Purely additive UI layer; no stored state or API breaks. Rollback = hide toolbar creation call. New Linux dependency appears only when print backend lands; ship toolbar + search first so the change stays bisectable.
