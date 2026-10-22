# Tasks

## 1. Shared engine fix

- [x] 1.1 In `src/mupdfengine.h` / `src/mupdfengine.cpp`, add
      `MuPdfEngine::resolveOutlineTarget()`: when `fz_outline::page` is
      invalid and `fz_outline::uri` is set, resolve the URI with
      `fz_resolve_link_dest()`, map the location through
      `fz_page_number_from_location()` to the public page number (including
      the MOBI cover shift) and derive the normalized `anchorY` from the XYZ
      destination; wrap the MuPDF calls in `fz_try`/`fz_catch` and keep the
      values across the block in `fz_var()` locals
- [x] 1.2 In `MuPdfEngine::outline()`, use the helper for every node, keep
      MuPDF's pre-resolved destination as the fast path, and set
      `OutlineItem::resolved` from the result so an unresolvable entry is
      reported as such instead of as page 0
- [x] 1.3 Verify with `harness-epub-toc` that the PDF outline (fast path) is
      unchanged and that the EPUB outline now resolves

## 2. Fixtures and harness

- [x] 2.1 Add `tools/generate_sample_epub_toc.py` generating
      `examples/epub-toc-nav3.epub` (EPUB3 navigation document) and
      `examples/epub-toc-ncx2.epub` (EPUB2 NCX), both with a nested entry and
      a fragment target; commit the generated fixtures
- [x] 2.2 Add a table of contents to the generated `examples/sample-links.pdf`
      (`tools/generate_sample_links.py`) so the harness can cover the fast
      path that the EPUB URI path must not regress
- [x] 2.3 Add `tests/harness_epub_toc.cpp` and the `harness-epub-toc`
      target in `CMakeLists.txt`: checks that every entry resolves to a page
      inside the document, that the fragment entry carries a mid-page anchor,
      that the in-page link to the same fragment resolves to the same page
      and position, and that activating a sidebar row navigates the real
      controller to the destination page
- [x] 2.4 Run `harness-epub-toc`, `harness-links` and `harness-epub21` and
      confirm they pass; re-run `harness-epub-toc` with the resolution
      disabled to confirm it fails (the harness is a real regression guard)

## 3. Documentation

- [x] 3.1 Record the fix in `AGENTS.md` (Known Issues → Fixed) and note the
      new fixtures and harness
- [x] 3.2 Update the `viewer-toolbar` capability delta so the outline
      sidebar requirement covers engine-resolved destinations and their
      in-page anchor
