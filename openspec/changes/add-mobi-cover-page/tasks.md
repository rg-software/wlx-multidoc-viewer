## 1. MOBI interception in `MuPdfEngine::open`

- [ ] 1.1 In `src/mupdfengine.h`: add a private `bool tryOpenMobi(fz_context* ctx, fz_stream* file, const QString& path);` (or `std::string`) that returns true when interception produced a document, false when the file is not a usable MOBI/PRC.
- [ ] 1.2 In `src/mupdfengine.cpp::open()`: keep the existing `fz_open_document_with_stream` as the final path; before it, when the extension is `.mobi`/`.prc`, call the intercept routine inside `fz_try`/`fz_catch` and use its result only on success. Wrap the whole intercept so any exception degrades to the existing plain open (see task 4.1).

## 2. Read buffer + extract archive

- [ ] 2.1 In the intercept routine: `fz_read_all(ctx, file, 0)` into an `fz_buffer` (mirrors `mobi_open_document` → `fz_read_all`); keep the stream ref until the buffer is built.
- [ ] 2.2 Declare the internal symbol locally as `extern "C" fz_archive *fz_extract_html_from_mobi(fz_context*, fz_buffer*);` and call it to build the tree archive. Drop the raw buffer afterwards (the archive owns its copies).

## 3. EXTH cover parsing + injection

- [ ] 3.1 Add a small byte-parsing helper (internal to `mupdfengine.cpp`) that, given the raw MOBI buffer:
    - reads the PalmDB record offsets (entries at 78-byte header offset, each `uint32` BE data offset + 4 attr/uid bytes);
    - parses record 0: 16-byte PalmDOC prefix + `"MOBI"` magic + header length → EXTH starts at `record0 + 16 + headerLength`.
- [ ] 3.2 From EXTH: parse `"EXTH"` magic, count, then records of `{type uint32, size uint32, data}`; locate type 201 (cover offset). Read the "First Image Index" at MOBI-header offset 0x6C; cover record index = `firstImageIndex + exth201 + 1`.
- [ ] 3.3 Read the cover record's bytes from the buffer; feed to `fz_recognize_image_format` and bail out (return false) unless it decodes as a recognizable image.
- [ ] 3.4 `fz_tree_archive_add_buffer(ctx, archive, "cover.jpg", coverBuf)`; then read `index.html` via `fz_read_archive_entry`, prepend `<div style="page-break-after: always"><img src="cover.jpg" style="width: 100%; height: auto;"/></div>`, and write the modified HTML back into a new buffer.
- [ ] 3.5 Build a local `fz_htdoc_format_t` constant `{"MOBI", NULL, 1, 1, FZ_HTML_FLAVOR_MOBI}` and `fz_htdoc_open_document_with_buffer(ctx, archive, html, &fmt)`. Hand both refs off (the document keeps its own); drop our archive/buffer refs in `fz_always`. Store the returned document.

## 4. Fallback and regression safety

- [ ] 4.1 Ensure every failure inside the intercept (`fz_catch`, unrecognized cover, missing EXTH, out-of-range indices, non-mobi magic) leaves the engine state untouched so `open()` continues to the unchanged `fz_open_document_with_stream` path. Verify this with the unit scenarios in section 6 before wiring 1.1/1.2.
- [ ] 4.2 Confirm non-MOBI formats (PDF, EPUB, DjVu, CHM, comics, images) still open solely via their existing paths — no behavior change, no shared-buffer mutation.

## 5. Build verification

- [ ] 5.1 Windows: `cmake --preset windows-x64-release && cmake --build --preset windows-release`; confirm `fz_extract_html_from_mobi` links (static lib) and no warnings from the `extern` declaration.
- [ ] 5.2 Linux: `cmake --preset linux-release && cmake --build --preset linux-release`; confirm the symbol resolves against system `libmupdf` (see risk R3 — if the distro hides it, record the link failure and the fallback decision in the change).

## 6. Smoke tests (Windows)

- [ ] 6.1 Open `examples/sample1.mobi` (EXTH[201]=0): expect 31 pages (was 30), page 1 = cover image, page 2 = existing page-1 copyright text; cover scales with fit/zoom and rotates like any page.
- [ ] 6.2 Traverse: next/prev/first/last/Jump to N account for the cover; search for a body word finds the text page, not the cover; outline (if any) targets the right pages; text selection works on body pages.
- [ ] 6.3 Fallback: open a MOBI with no EXTH[201] (or corrupt the cover record in a copy) — page count stays 30, page 1 is the body as today, no crash, no error dialog.
- [ ] 6.4 Regression: re-open a PDF and an EPUB and confirm identical page counts and first-page rendering vs. before the change; confirm DjVu/CHM/comic/image paths unaffected.
- [ ] 6.5 Copy `MultidocViewer.wlx64` alongside Total Commander on Windows and repeat 6.1/6.2 in the lister panel (double-check the plugin loads and pages render).

## 7. MuPDF version pin note

- [ ] 7.1 Update `AGENTS.md` (or the change summary) to record that the engine relies on the non-public symbol `fz_extract_html_from_mobi`, so any future libmupdf bump must re-verify its export and this change's task list (link to risk R1/R3).