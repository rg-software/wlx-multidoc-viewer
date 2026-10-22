# Proposal

## Why

An EPUB table of contents shows up in the sidebar but every entry is inert:
clicking one does nothing, while Ctrl+click on the same target inside the page
navigates correctly. Reproduced with `epub_probe_notes_example.epub` (8 pages,
6 TOC entries) and with the new `examples/epub-toc-*.epub` fixtures.

Root cause: MuPDF's EPUB outline loader (`source/html/epub-doc.c`, the NCX
parser at line 744 and the EPUB3 nav parser at line 825) stores the
destination only in `fz_outline::uri` and leaves `fz_outline::page` at
`fz_make_location(-1, -1)`. Every other MuPDF outline loader resolves the
destination up front — PDF via the generic loader (`source/fitz/outline.c`),
XPS via `source/xps/xps-outline.c`, HTML/MOBI via `htdoc_update_outline`
(`source/html/html-doc.c`) — so `MuPdfEngine::outline()` could read
`node->page` directly. For EPUB that field is invalid, every entry's `pageNo`
became 0, and `ViewerController::goToPage(0)` clamped it to page 1: a silent
no-op. The sidebar still listed the entries because they were resolved by the
TOC parser, not by the destination.

EPUB is the only MuPDF format with this shape; the in-page link machinery
(`pageLinks()`) already resolved the same URIs correctly, which is why only
the sidebar was broken.

## What Changes

- `MuPdfEngine::outline()` resolves an outline entry whose destination MuPDF
  left unresolved: the raw `fz_outline::uri` is resolved with
  `fz_resolve_link_dest()` — the same call `pageLinks()` already makes — and
  the resulting location is mapped to a public page number.
- The resolved entry also carries a normalized in-page anchor, so a TOC entry
  that points at a fragment (`#sec-1-2`) lands on that fragment instead of
  the top of the page.
- Entries whose destination cannot be resolved are marked unresolved instead
  of silently pointing at page 0, so the sidebar can skip them when tracking
  the reading position.
- New fixtures (`examples/epub-toc-nav3.epub`, `examples/epub-toc-ncx2.epub`)
  cover the EPUB3 nav document and the EPUB2 NCX, and a new headless harness
  (`harness-epub-toc`) verifies the resolution, the anchor, the link
  cross-check and the sidebar activation path.

## Capabilities

### Modified Capabilities
- `viewer-toolbar`: the outline sidebar requirement now covers destinations
  that the engine has to resolve itself (EPUB tables of contents) and the
  in-page anchor they carry.

### Affected Platforms
Both. The fix is in the shared `MuPdfEngine`; the Win32 viewer on Windows and
the Qt viewer on Linux consume the same outline model.
