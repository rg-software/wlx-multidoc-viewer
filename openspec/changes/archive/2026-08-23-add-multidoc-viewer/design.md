## Context

This is a greenfield project — no existing code. The WLX plugin interface is a simple C ABI (4 exported functions) where the plugin creates a child QWidget under the host window. We need to render documents from two native libraries (MuPDF and DjVuLibre) into QImages and display them in a Qt6 widget with navigation/zoom controls.

The WLX interface requires that each plugin instance is self-contained (no global mutable state shared across instances), and that the plugin returns a window handle from `ListLoad` that Double Commander manages.

## Goals / Non-Goals

**Goals:**
- Render all MuPDF-supported formats + DjVu into navigable viewer widgets
- Cross-platform build (Linux primary, Windows secondary)
- Minimal binary footprint (~5-8MB total including bundled libs)
- Clean separation between rendering engines, UI, and plugin glue
- Each document instance fully isolated (own context, own widget)

**Non-Goals:**
- Annotation editing (view-only)
- Text search / find-in-document
- Printing support
- Form filling
- Bookmarks / outline sidebar navigation
- Dark mode / recoloring
- Thumbnail generation for file manager icons
- Office document rendering (DOCX/ODT) — deferred to future work
- JavaScript-capable markdown rendering — deferred to future work

## Decisions

### D1: Rendering architecture — render-to-QImage, not embedded widget

**Decision:** Each engine renders pages to `QImage` (Format_RGB888). The UI displays the QImage in a QLabel inside a QScrollArea.

**Alternatives considered:**
- *Embed MuPDF's fitz viewer widget*: MuPDF doesn't ship a Qt widget. The iOS/Android viewers use custom rendering. Not applicable.
- *Use QPdfView (Qt6 Pdf module)*: Only handles PDF. Doesn't cover DjVu, EPUB, etc. Defeats the purpose.
- *Use QWebEngineView*: Heavy dependency, overkill for non-HTML formats.

**Rationale:** QImage rendering is format-agnostic — both MuPDF (fz_pixmap) and DjVuLibre (rendered pixel buffer) produce raw RGB pixels that map directly to QImage. This keeps the UI layer simple and uniform across all formats.

### D2: Engine abstraction — virtual interface with two backends

**Decision:** Define an abstract `DocumentEngine` interface with methods like `open()`, `renderPage()`, `pageCount()`, `extractText()`, `metadata()`, `close()`. Implement `MuPdfEngine` and `DjVuEngine` concrete classes.

```
DocumentEngine (abstract)
  ├── MuPdfEngine    (wraps fz_context / fz_document)
  └── DjVuEngine     (wraps ddjvu_document_t)
```

**Alternatives considered:**
- *Free functions with opaque handles*: Less type-safe, harder to manage lifetime.
- *Single engine with format detection inside*: Would make MuPDF and DjVu code entangled. Two backends keep concerns separate.

**Rationale:** Classic strategy pattern. The UI widget holds a `std::unique_ptr<DocumentEngine>` and doesn't know which backend is active. Adding a new backend (e.g. LibreOfficeKit for office docs) means adding a new class, not modifying existing code.

### D3: MuPDF context management — one fz_context per document

**Decision:** Each `MuPdfEngine` instance creates and owns its own `fz_context*`. The context is created in the constructor and destroyed in `close()`.

**Alternatives considered:**
- *Shared global fz_context with locking*: MuPDF contexts are not thread-safe by default. Sharing would require external synchronization and defeats the isolation guarantee. MuPDF's own documentation recommends per-thread or per-document contexts.

**Rationale:** Simplest correct approach. Each WLX instance is independent, so each gets its own context. Memory overhead is negligible (~100KB per context).

### D4: Image conversion — direct memory mapping

**Decision:** Convert `fz_pixmap` to `QImage` by wrapping the pixel buffer. MuPDF renders to RGB888 which maps directly to `QImage::Format_RGB888`. Use `QImage::Format_RGB888` with a copy of the data (not a wrap, since fz_pixmap lifetime is short).

For DjVuLibre: allocate a buffer, call `ddjvu_page_render()`, then construct a `QImage` from the buffer.

**Alternatives considered:**
- *Render to RGBA*: Extra byte per pixel, no benefit since we don't need alpha.
- *Render to grayscale then convert*: Loses color information unnecessarily.

**Rationale:** RGB888 is the natural output of both MuPDF and DjVuLibre's color rendering. No format conversion needed.

### D5: UI layout — QToolBar + QScrollArea + QLabel

**Decision:**
```
┌──────────────────────────────────────────────┐
│ QToolBar (navigation, zoom, fit, info)       │
├──────────────────────────────────────────────┤
│ QScrollArea                                   │
│   └─ QLabel (displays QImage, scaled)        │
└──────────────────────────────────────────────┘
```

The QLabel's pixmap is set to the rendered QImage. The QScrollArea handles scrolling when the page is larger than the viewport. Zoom is achieved by scaling the QImage before display (not by scaling the QLabel).

**Alternatives considered:**
- *Custom QWidget with paintEvent*: More control but more code. QLabel + QScrollArea is sufficient for a viewer.
- * QGraphicsView/QGraphicsScene*: Overkill for displaying a single image per page.

**Rationale:** QScrollArea + QLabel is the simplest Qt pattern for displaying a scrollable, zoomable image. It's well-tested and requires minimal code.

### D6: Zoom implementation — scale QImage, not widget

**Decision:** Re-render the page from the engine at the target DPI/zoom factor. MuPDF accepts a matrix transform for scaling. DjVuLibre renders at a specified DPI. The engine returns a QImage at the exact pixel dimensions needed.

**Alternatives considered:**
- *Render at native resolution, then QImage::scaled()*: Faster (no re-render), but blurry at non-native zoom levels. MuPDF's scaling is resolution-aware and produces sharper results.

**Rationale:** Re-rendering through the engine produces the sharpest output at every zoom level. MuPDF's scaler is optimized and fast enough for interactive use. For DjVu, we control the DPI parameter directly.

### D7: Build system — CMake with pkg-config

**Decision:** Use CMake with `find_package(PkgConfig)` and `pkg_check_modules` for MuPDF and DjVuLibre discovery. Qt6 via `find_package(Qt6)`.

**Alternatives considered:**
- *Premake5 (like SumatraPDF)*: Non-standard, harder for contributors.
- *Meson*: Good but less common in the Qt ecosystem.
- *qmake*: Deprecated in Qt6.

**Rationale:** CMake is the de facto standard for C++/Qt6 projects. pkg-config handles the non-Qt dependencies cleanly on Linux. On Windows, vcpkg or manual paths can be substituted.

### D8: Supported extension list — compile-time constant

**Decision:** The full list of supported extensions is a compile-time array of strings used by `ListGetDetectString` and the format dispatcher. Adding a new format means adding to this array and ensuring the engine supports it.

**Alternatives considered:**
- *Runtime registration*: More flexible but adds complexity. Not needed for a fixed set of formats.

**Rationale:** The format set is stable (MuPDF's supported formats change slowly). A compile-time list is simple, auditable, and generates the detect string deterministically.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| **MuPDF AGPL-3.0 license** requires source disclosure if distributed as a service | Plugin is distributed as a binary with source — GPL-3.0 compatible. Document the license obligation clearly. |
| **DjVuLibre is GPL-2.0+**, which is compatible with GPL-3.0 but not AGPL-3.0 | DjVuLibre is a separate library, not linked into MuPDF. Plugin is GPL-3.0 which satisfies GPL-2.0+ requirement. |
| **EPUB/MOBI layout quality** may be poor compared to dedicated ebook readers | Acceptable for a preview plugin. Users can open in a dedicated reader for full experience. |
| **MuPDF version coupling** — ext/ bundled libraries must match the MuPDF version | Pin MuPDF version in CMake. Document exact compatible versions. |
| **Memory usage** — each open preview instance holds a full fz_context + decoded pages | Reasonable for 1-3 simultaneous previews. Document that heavy use may increase memory. |
| **No text search** in initial version | Explicitly listed as non-goal. Can be added later via `fz_search_page`. |
| **DjVuLibre rendering speed** for large pages | DjVu rendering is generally fast. If issues arise, cache rendered pixmaps. |
| **Windows build complexity** — MuPDF and DjVuLibre need to be compiled or obtained | Provide vcpkg manifest or document build steps. Linux is primary target. |

## Open Questions

- **Bundled vs system libraries:** Should the plugin bundle MuPDF and DjVuLibre as static libraries, or expect them to be installed system-wide? This affects distribution complexity and binary size. (Can be decided during implementation — start with system libs via pkg-config, add bundling option later if needed.)
- **Continuous scroll vs single-page display:** The spec defines single-page display. Should continuous scroll (showing multiple pages in sequence) be in the initial version or deferred? (Doesn't affect specs — it's a UI polish item that can be added without changing the engine interface.)
