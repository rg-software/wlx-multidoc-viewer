## Context

See `proposal.md` — Why/What. Verified facts the design builds on:

- `fz_open_document_with_stream` reads the whole MOBI into an `fz_buffer` (`mobi_open_document` → `fz_read_all`), then `fz_extract_html_from_mobi` converts it into a tree archive: `index.html` (decompressed text) plus image records keyed by opaque `"%05d"` names. The result is opened as `fz_htdoc_open_document_with_buffer(..., &fz_htdoc_mobi)` and laid out lazily — so the former two functions are the natural interception point *before* layout.
- `fz_extract_html_from_mobi`, `fz_new_tree_archive`, `fz_tree_archive_add_buffer`, `fz_read_archive_entry`, and `fz_htdoc_open_document_with_buffer` all exist in the pinned 1.28.3 build (confirmed in `build/release/vcpkg_installed/.../include/mupdf`). The `fz_htdoc_mobi` format constant is internal, but its fields are public and trivially replicated: `{"MOBI", NULL, 1, 1, FZ_HTML_FLAVOR_MOBI}`.
- `fz_htdoc_open_document_with_buffer` keeps its own refs to the archive and buffer (confirmed in `html-doc.c`: callers drop both after the call), so ownership is a plain hand-off.

## Goals / Non-Goals

**Goals:**
- Render a MOBI/PRC EXTH cover as a real page 1 ahead of the body, with no page-index shifting anywhere in the engine or viewer.
- Use only public MuPDF APIs plus a single forward-declared internal symbol, and pin the behavior to MuPDF 1.28.3.
- Fall back gracefully — a MOBI with no usable cover opens exactly as today; any interception failure degrades to the current plain-open path.

**Non-Goals:**
- EPUB cover handling (MuPDF's `epub.c` ignores OPF cover metadata too, but the common case — an `<img>` on the first spine page — already renders; out of scope).
- Reimplementing MuPDF's PalmDOC/MOBI record decoding (decompression, encodings, huff/cdic). We reuse `fz_extract_html_from_mobi` rather than rewrite it.
- Viewer-side virtual cover pages, toolbar/UI changes, or any per-platform viewer code.

## Decisions

**D1 — Intercept before layout by reading the MOBI into a buffer ourselves, calling the internal `fz_extract_html_from_mobi`, and reopening via the public `fz_htdoc_open_document_with_buffer`.**
This mirrors MuPDF's own `mobi_open_document_with_buffer` exactly. We open the file as an `fz_stream`, `fz_read_all` into a buffer, call `fz_extract_html_from_mobi` (declared locally as `extern "C" fz_archive *fz_extract_html_from_mobi(fz_context*, fz_buffer*)` — it is a non-static exported symbol in the pinned build), then modify the returned tree archive and HTML before reopening. Because the cover is in the document before the first page is ever laid out, `fz_count_pages`, navigation, search, selection, dimensions, and outline all work without any adjustment. PDF/EPUB/etc. keep the unchanged `fz_open_document_with_stream` path.
*Alternatives rejected:* reimplementing the MOBI decoder (duplicates PalmDOC logic, ~200+ lines of risky format code); virtual cover page with index shifting (touches every page-indexed engine method); casting into the private `html_document` struct (version-fragile).

**D2 — Parse EXTH ourselves from the raw file buffer, not from MuPDF state.**
The cover lives in as few as three byte ranges: the PalmDB record offsets (entry = 4-byte BE offset + 4 attribute/uid bytes, starting after the 78-byte PDB header), the MOBI header inside record 0 (PalmDOC 16-byte prefix + `"MOBI"` magic, header length field; EXTH starts at `record0 + 16 + headerLength`, verified on `examples/sample1.mobi`), and the EXTH header itself (`"EXTH"` magic, count, then records of type/size/data). Type 201 carries the cover's record offset relative to the "First Image Index" field (MOBI-header offset **0x5C**, not 0x6C — 0x6C is Huffman Table Length). Resolving `coverIndex = firstImageIndex + exth201` gives the PalmDB record whose bytes we read as the cover image (`sample1.mobi`: 20 + 0 → record 20, a 162,859-byte JFIF JFIF.jpg signature confirmed). We also confirm the EXTH-present bit (`EXTH flags` field at MOBI-header offset 0x70, bit 0x40) before trusting 0x5C. This is pure byte parsing — no MuPDF involvement, no new dependency.
*Alternatives rejected:* asking MuPDF for the cover (it has no such concept); mapping EXTH values onto MuPDF's opaque `"%05d"` archive names (fragile coupling to internal numbering).

**D3 — Add the cover to the archive under our own fixed name and prepend an `<img>` to the HTML.**
Instead of mapping the cover record onto MuPDF's `"%05d"` archive names, we read the cover bytes from the raw file (D2), `fz_tree_archive_add_buffer(ctx, archive, "cover.jpg", coverBuf)`, and prepend `<div style="page-break-after:always"><img src="cover.jpg" style="width:100%;height:auto"/></div>` to the `index.html` buffer before reopening. This puts the cover on its own page (forced page break), ahead of the body, without needing to know how MuPDF numbers image records. We sniff the format with `fz_recognize_image_format` on the cover bytes and only bundle them when they decode as an image; otherwise we skip injection entirely.
*Alternatives rejected:* computing the `"%05d"` name (internal numbering may skip non-images, off-by-one risk); absolute page injection after layout (breaks text/search ranges).

**D4 — Everything happens inside `MuPdfEngine::open`, before `fz_count_pages`, guarded so any failure degrades to the current open path.**
The open path becomes: detect `.mobi`/`.prc` → try interception via D1–D3 inside `fz_try` → on success proceed normally; if the suffix is not mobi/prc, or interception throws, fall through to the existing `fz_open_document_with_stream` exactly as today. `open()` therefore never regresses a currently-working file.
*Alternatives rejected:* a dedicated engine/subclass (over-engineering for one suffix path).

## Platform-Specific Code

- **Shared (`src/mupdfengine.cpp`, `src/mupdfengine.h`):** the entire change is platform-agnostic — buffer parsing is byte math, and all MuPDF calls are the same on both platforms. The `extern "C"` declaration of `fz_extract_html_from_mobi` lives in `mupdfengine.cpp` and is used on both Windows (static lib, symbol guaranteed in our overlay build) and Linux (system lib — see risk R3).
- **Windows:** none beyond the shared path. The static libmupdf built by our overlay port always exports the symbol.
- **Linux:** none beyond the shared path. System `libmupdf` is invoked with the same public APIs.
- **No viewer changes:** `viewer_win32.*` / `viewer.*` / `viewercontroller.*` / toolbar / sidebar / print are untouched on both platforms — the cover is simply page 1 of the document.

## Risks / Trade-offs

- [R1: `fz_extract_html_from_mobi` is not a documented public symbol] → Mitigation: it is non-static and exported by the pinned build on both platforms; we pin MuPDF 1.28.3 and the AGENTS.md already documents overlay-version coupling. A future MuPDF bump must re-verify the symbol (add to the change's tasks).
- [R2: EXTH field offsets (0x6C first-image-index, 201 type, PalmDB header layout) are spec-derived] → Mitigation: off-by-one/offset errors fail *open* — if the cover record doesn't `fz_recognize_image_format` or indices are out of range, we skip injection and the file opens unchanged. Covered by the "no cover" spec scenarios.
- [R3: Linux system libmupdf may hide internal symbols] → Mitigation: MuPDF ships default visibility (no `-fvisibility=hidden`), the symbol is reachable; if a distro ever hides it, the build breaks loudly at link time rather than silently at runtime, and the fix is a one-line fallback to the plain open path.
- [R4: `fz_tree_archive_add_buffer` re-keys the archive] → MuPDF's own mobi path does the same kind of tree mutation; adding `cover.jpg` before `fz_htdoc_open_document_with_buffer` consumes it. The archive is ours until the document keeps its own ref, so no stale references.
- [R5: cover aspect ratio / fit] → The `<img>` is `width:100%` with auto height and a forced page break; fit modes and zoom apply to it like any page through the existing viewer. Extreme aspect ratios may letterbox — same behavior Sumatra/Kindle show.

## Migration Plan

No data migration; single in-place code change in the engine. Rollback = revert the commit. The change is additive: without the interception the old path is byte-identical, so the Windows static build and Linux system build both have a trivial fallback.

## Open Questions

None — spec, approach, and task breakdown are settled (see D1–D4 and `tasks.md`).