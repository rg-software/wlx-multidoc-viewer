## Why

The viewer paints documents as flat bitmaps — users can read but never copy anything out of a PDF, XPS, EPUB, or DjVu file shown in the lister. Selecting and copying a snippet is table stakes for any document viewer (SumatraPDF parity) and both backends already carry the needed data: MuPDF builds positioned text (`fz_stext_page`) on every extraction today and throws the geometry away, while DjVuLibre exposes per-word boxes via `ddjvu_document_get_pagetext`.

## What Changes

- `DocumentEngine` gains a geometry-aware text API: per-page glyph/word items with bounding quads plus a flag telling whether a page has a selectable text layer.
- MuPDF engine surfaces quad data from the `fz_stext_page` it already builds.
- DjVu engine implements text-layer extraction by parsing `ddjvu_document_get_pagetext` (word granularity) — currently a stub.
- Shared selection model in `ViewerController`: anchor/focus glyph positions, selected ranges, extracted text, screen-space highlight rectangles.
- Both viewers (Windows Win32 and Linux Qt) get: I-beam cursor over selectable text, drag-select with highlighted ranges spanning pages in continuous mode, `Ctrl+C` copy to clipboard, `Esc` clears selection. Left-drag that does not start on selectable text keeps its current drag-pan behavior (SumatraPDF-style split).
- Selection resets on zoom, rotation, mode toggle, page navigation away, and document close/reload.

## Capabilities

### New Capabilities
- `viewer-interaction/text-selection`: Detecting selectable text layers, cursor feedback, mouse range selection with visual highlight, clipboard copy, and selection lifetime rules.

### Modified Capabilities

## Impact

- `src/document.h` — new text-item structs and engine methods.
- `src/mupdfengine.*`, `src/djvuengine.*` — geometry-aware extraction (DjVu is net-new code).
- `src/viewercontroller.*` — selection state machine and device↔page coordinate mapping shared by both platforms.
- `src/viewer_win32.*`, `src/viewer.*` — hover cursor, mouse capture, overlay painting, clipboard.
- No new dependencies; static-link footprint unchanged.
