## Context

`formatdispatcher.cpp::isRasterSuffix` currently routes `tif/tiff/webp` to `ImageEngine` and `QImageReader`, but this Qt build (trimmed `qtbase` overlay port) installs only `qgif`/`qico`/`qjpeg` image plugins plus built-in png/bmp; there is **no TIFF or WEBP plugin** in `Qt6/plugins/imageformats` (verified: only `qgif.lib/qico.lib/qjpeg.lib`). So `ImageEngine` cannot decode `.tif/.tiff/.webp` today. On initial open they reach MuPDF via the `openDocument` fallback; during folder browse `openSibling` omits that fallback and dead-ends after still listing them as siblings — the dishonesty the user flagged. See proposal.md – Why.

## Goals / Non-Goals

**Goals:**
- The image folder contains only formats `ImageEngine` decodes natively (`jpg/jpeg/png/gif/bmp/ico`), so every listed sibling is a real, single-page image.
- TIFF is handled as a document through the MuPDF engine (multi-page paged doc, one page per IFD), grouped with PDF/CBR/CB7.
- WEBP is dropped (no decoder in this build); support is a tracked follow-up.
- Remove the now-unreachable raster→MuPDF fallback in the controller.

**Non-Goals:**
- No WEBP decoder (neither Qt `QImageReader` nor MuPDF has one in the trimmed build; adding it means a new `libwebp`/webp-plugin build of the `qtbase` overlay — a separate change).
- No in-place TIFF animation (TIFF is a paged document, which is the more correct behavior).
- No change to `ComicEngine` or document routing for existing formats.

## Decisions

### D0. Use the verified MuPDF TIFF document path

Confirmed against the 1.28.3 source: MuPDF's image-document handler (`img_extensions` includes `tif`/`tiff`, `source/cbz/muimg.c:288-289`) routes a `.tiff` through `img_open_document`, which counts every TIFF IFD via `fz_load_tiff_subimage_count` as `page_count` and loads each IFD as a distinct page via `fz_load_tiff_subimage` (`muimg.c:196-201`). `MuPdfEngine::open` calls `fz_open_document_with_stream(path, ...)`, whose magic/extension matching (`document.c:316-323`) gives the img handler a 100-score for a `.tiff` suffix. So multi-page TIFF is genuinely supported (one page per IFD), not first-frame-only.

### D1. Drop `tif/tiff/webp` from the raster sets; TIFF falls through to MuPDF, WEBP is unrouted

Narrow `formatdispatcher::isRasterSuffix` to `jpg/jpeg/png/gif/bmp/ico` and `imagefolder::isRasterName` to the same set. Because `createEngine` already defaults unknown suffixes to `MuPdfEngine`, `.tif/.tiff` automatically route to MuPDF once removed — no explicit TIFF-branch needed, exactly like `.pdf`. WEBP also falls out of the raster set; since nothing decodes it, `.webp` simply isn't a supported extension (opening it shows no document) until a webp decoder is added.

- Alternatives:
  - an explicit `TiffEngine`/`WebpEngine` subclass — rejected, MuPDF already exposes page semantics and would be a wrapper with no added behavior;
  - adding the Qt webp plugin + `libwebp` now — rejected as out of scope (vcpkg overlay + full rebuild), tracked as a follow-up (`add-webp-image-support`).

### D2. Add `ico`

`qico` is one of the three installed image plugins, so `.ico` is genuinely natively decodable and belongs in the roster. Low risk; matches "native decoders only".

### D3. Remove the controller raster→MuPDF fallback

`openDocument`/`openSibling` currently retry with `MuPdfEngine` when a raster-open fails. With TIFF/WEBP no longer routed to `ImageEngine`, that retry is unreachable for them. For genuine native rasters the user is opening, a decode failure now simply shows no document (consistent, honest). Remove `isRasterPath` and the retry branch to delete the dead path.

- Trade-off: a corrupt `.jpg` that Qt can't decode no longer tries MuPDF. Acceptable — the op is homogeneous, and MuPDF likely can't decode a corrupt jpg either.

## Risks / Trade-offs

- **R1: Users who relied on folder-browsing a TIFF lose that** — a TIFF must be opened directly (as with any document). Mitigation: the previous browse path for them was broken anyway (dead-ended), so this is a strict improvement and matches document semantics.
- **R2: WEBP is unsupported** — a `.webp` opens as no document until an webp decoder is added to the build (follow-up). This matches reality (no decoder exists) and avoids advertising a format that cannot open.
- **R3: `.ico` in the roster** — harmless, decodes natively; if a user dislikes ico in a photo folder it is easily dropped later.
- **R4: Corrupt native image no longer falls back to MuPDF** — see D3. Acceptable.

## Migration Plan

Routing-only bugfix + collapse; no data/config migration. Commit as a conventional `fix:`/`feat:` scoped to images/docs (e.g. `feat(images): route TIFF as document and keep native-only image roster`). Rollback is a revert. AGENTS.md's image-engine note must be updated to the narrowed roster, the TIFF-as-document rule, and the WEBP gap row.