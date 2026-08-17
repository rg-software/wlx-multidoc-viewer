## Why

Double Commander's file preview (Quick View panel / F3 lister) currently lacks a native multi-document viewer plugin. The existing `qtpdfview_qt` plugin handles only PDF via Qt's Pdf module. Users working with DjVu, ebooks (EPUB/MOBI/FB2), comic book archives (CBZ/CBR/CB7), and various image formats have no built-in preview capability. SumatraPDF is the gold standard for multi-format viewing on Windows, but it's a standalone Win32 application — not embeddable as a Double Commander plugin.

The goal is to create a cross-platform WLX plugin that previews the same document classes as SumatraPDF, using MuPDF as the native rendering engine and DjVuLibre for DjVu files.

## What Changes

- **New WLX plugin** (`wlx-multidoc-viewer.so` / `.dll`) that registers as a Double Commander content plugin
- **MuPDF backend** handling PDF, XPS, EPUB, MOBI, FB2, CBZ/CBR/CB7/CBT, HTML, Markdown, plain text, and image formats (JPEG, PNG, TIFF, GIF, BMP, WebP, AVIF, JXL, TGA, PSD)
- **DjVuLibre backend** handling DjVu files
- **Qt6 viewer widget** with toolbar for page navigation, zoom controls, fit-to-width/page toggle, and document info display
- **Format dispatcher** that detects file type and routes to the appropriate rendering engine
- **Cross-platform build system** (CMake) targeting Linux and Windows, with pkg-config for library discovery

## Capabilities

### New Capabilities

- `plugin/wlx-entry-points`: WLX plugin interface implementation — ListLoad, ListLoadNext, ListCloseWindow, ListGetDetectString entry points; file type detection via extension matching; widget lifecycle management
- `engine/mupdf-renderer`: MuPDF-based rendering engine — document loading, page rendering to QImage, text extraction, outline/TOC loading, zoom/rotation transforms, page count and metadata access
- `engine/djvu-renderer`: DjVuLibre-based rendering engine — DjVu document loading, page rendering to QImage, text extraction, page dimensions
- `ui/viewer-widget`: Qt6 viewer widget — toolbar with page navigation and zoom controls, page display area (QLabel-based), fit-to-width and fit-to-page zoom modes, single-page and continuous scroll modes, document info dialog

### Modified Capabilities

(none — this is a greenfield project)

## Impact

- **New dependencies**: MuPDF (libmupdf + bundled ext/ libraries), DjVuLibre (libdjvulibre), Qt6::Widgets
- **Build system**: CMake-based, requires pkg-config for MuPDF and DjVuLibre discovery
- **Binary size**: ~5-8MB shared library (plugin .so/.dll)
- **No existing code affected**: The project currently contains only OpenSpec configuration — no application code to modify
- **License**: Plugin will be GPL-3.0 (compatible with MuPDF's AGPL-3.0 and DjVuLibre's GPL-2.0+)
- **Distribution**: Plugin binary + MuPDF/DjVuLibre shared libraries bundled, or expected to be system-installed
