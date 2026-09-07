## 1. MOBI interception in `MuPdfEngine::open`

- [x] 1.1 In `src/mupdfengine.h`: add a private `bool tryOpenMobi(fz_stream* file);` that parses the EXTH cover record and stores a decodable cover into the engine (cover page).
- [x] 1.2 In `src/mupdfengine.cpp::open()`: for `.mobi`/`.prc`, call the intercept routine before the plain `fz_open_document_with_stream`; the cover is detected but the DOCUMENT is **always** opened via the plain path (section page breaks remain byte-identical). On success `m_bodyHasCover` makes public page 1 the synthetic cover; on any failure `m_bodyHasCover=false` and there is no cover page.
- [x] 1.3 **Design change (verified empirically):** cover is a **synthetic engine-side page**, NOT an HTML injection. Injecting a cover element into `index.html` (any placement — prepend, after `</head>`, first child of `<body>`, with/without `page-break-after`) makes MuPDF's restartable layout drop the subsequent `mbp:pagebreak` page-break-before elements, collapsing section breaks (Contents/for-Sharon/Chapter boundaries). Keeping the body document byte-identical preserves them exactly (verified: intercept p02.. correspond 1:1 to plain fb01.. after the cover).

## 2. Read buffer + extract archive

- [x] 2.1 In the intercept routine: `fz_read_all(ctx, file, 0)` into an `fz_buffer` (mirrors `mobi_open_document` → `fz_read_all`); keep the stream ref until the buffer is built, then rewind so the plain open can re-read.
- [x] 2.2 No archive extraction is needed anymore (removed `fz_extract_html_from_mobi` usage): the cover is decoded directly from the record bytes. Kept `extern "C"` declaration history in git only.

## 3. EXTH cover parsing, engine-side cover page

- [x] 3.1 Byte-parsing helper (internal to `mupdfengine.cpp`) that reads the PalmDB record offsets (entries at 78-byte header offset, each `uint32` BE data offset + 4 attr/uid bytes) and parses record 0: 16-byte PalmDOC prefix + `"MOBI"` magic + header length → EXTH starts at `record0 + 16 + headerLength`.
- [x] 3.2 From EXTH: parse `"EXTH"` magic, count, then records of `{type uint32, size uint32, data}` (the `size` field INCLUDES the 8-byte type/size header — advance by `size`, not `size+8`); locate type 201 (cover offset). Read the "First Image Index" at MOBI-header offset 0x5C (verify EXTH-present bit at offset 0x70 first); cover record index = `firstImageIndex + exth201` (no +1 — verified on `examples/sample1.mobi`: 20 + 0 → record 20). Range-and-image-format checks (`fz_recognize_image_format != FZ_IMAGE_UNKNOWN`) bail out otherwise.
- [x] 3.3 Decode the cover record bytes with `QImage::fromData` (same codec path as the comic engine); an undecodable image means no cover page.
- [x] 3.4 N/A (no HTML injection / tree archive staging).
- [x] 3.5 `renderPage(1)` draws the stored cover scaled to fit (contain, centered) the body page area at the requested zoom (source: `pageDimensionsRaw(1)`), so the cover matches the body text region; rotation handled like the body render. `pageText`/`extractText`/`searchText(1)` return empty; `pageDimensions(1)` = body size; `outline` page numbers shift +1.

## 4. Fallback and regression safety

- [x] 4.1 Every failure inside the intercept (`fz_catch`, unrecognized cover, missing EXTH, out-of-range indices, non-mobi magic) leaves `m_bodyHasCover=false` so open() and the plain path behave unchanged. Only `FZ_ERROR_SYSTEM` propagates. Verified by the corrupted-cover scenario in section 6.
- [x] 4.2 Non-MOBI formats (PDF, EPUB, DjVu, CHM, comics, images) still open solely via their existing paths — the intercept is gated on the `.mobi`/`.prc` suffix only.

## 5. Build verification

- [x] 5.1 Windows: `BuildMakeSetup.bat` (x64 + x86) builds clean.
- [ ] 5.2 Linux: `cmake --preset linux-release && cmake --build --preset linux-release`; confirm behavior (no `fz_extract_html_from_mobi` dependency anymore, so no external-symbol risk remains).

## 6. Smoke tests (Windows)

- [x] 6.1 Headless (new `harness-mobi`, built with `WLX_BUILD_HARNESS=ON`): open `examples/sample1.mobi` (EXTH[201]=0) → 24 pages (23 body + 1 cover), page 1 = cover image (no text layer, ~100% opaque coverage, 420x595 at zoom 1), page 2 = copyright. Section page breaks preserved: p03 = Contents, p04 = for-Sharon … identical to plain open shifted by the cover.
- [x] 6.2 Headless: page 2 renders, whole-document search finds body text on page 2, outline targets in range, cover page renders non-blank.
- [x] 6.3 Fallback (headless): a copy with the First-Image-Index at 0x5C rewritten to 0xFFFFFFFF → intercept bails, plain open yields the original 23 body pages, no crash.
- [ ] 6.4 Regression (interactive): re-open a PDF and an EPUB and confirm identical page counts and first-page rendering vs. before the change; confirm DjVu/CHM/comic/image paths unaffected.
- [ ] 6.5 Interactive: copy `MultidocViewer.wlx64` alongside Total Commander on Windows and repeat 6.1/6.2 in the lister panel (double-check the plugin loads, cover shows on page 1, and section page breaks are visibly preserved).

## 7. MuPDF version pin note

- [x] 7.1 Update `AGENTS.md`: record that the engine no longer depends on the non-public symbol `fz_extract_html_from_mobi` (synthetic-cover approach removes risk R1/R3), and that HTML injection into reflowable htdocs drops body `mbp:pagebreak` section breaks — do NOT reintroduce it.