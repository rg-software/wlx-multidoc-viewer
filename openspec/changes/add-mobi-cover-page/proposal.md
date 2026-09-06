## Why

MOBI/PRC ebooks opened in the lister show no cover page even when the file has one: MuPDF's MOBI handler (`source/html/mobi.c`) never reads the EXTH cover metadata (type 201) and only renders images that are referenced by an `<img>` tag in the body. Most MOBI bodies have no `<img>` for the cover, so the cover image — which exists in the file — is silently dropped. SumatraPDF's own MOBI parser reads EXTH[201] and emits a cover, so its 29-page result looks different from our 30-page one. We should surface the cover like Sumatra does.

## What Changes

- For **MOBI** and **PRC** files opened by the MuPDF engine, the engine parses the raw file's EXTH header to find the cover image record (EXTH type 201), and injects it into the document so it renders as **page 1**.
- The cover is incorporated **before** layout by prepending an `<img>` reference to the MOBI's `index.html` buffer and opening the resulting document with MuPDF's public `fz_htdoc_open_document_with_buffer` API. This makes the cover a natural first page: `pageCount`, text extraction, search, dimensions, and outline targets all keep working without any page-index shifting.
- When a MOBI has no cover (no EXTH[201], or the referenced record is not an image), behavior is unchanged — the file renders exactly as today.
- Because the cover is a genuine page, it participates in every existing viewer feature (paged/continuous, fit, zoom, rotation, selection, search, outline) on **both** Windows and Linux with no viewer-side changes.

## Capabilities

### New Capabilities
- `mobi-format`: MOBI/PRC opening in the lister, including the cover page surfaced from EXTH metadata as page 1 and fallback when no cover exists.

### Modified Capabilities
- none

## Impact

- `src/mupdfengine.cpp` / `src/mupdfengine.h` — the MOBI open path adds a pre-layout EXTH parse + cover `<img>` injection using public MuPDF APIs (`fz_extract_html_from_mobi`, `fz_read_archive_entry`, `fz_htdoc_open_document_with_buffer`). PDF/EPUB/etc. keep using `fz_open_document_with_stream` unchanged.
- No new dependencies, no viewer-toolbar/sidebar/print changes.
- Append `mobi-format` to the project's capability specs.
