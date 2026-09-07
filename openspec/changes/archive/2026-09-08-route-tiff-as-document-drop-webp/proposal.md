## Why

The image folder feature lists every raster suffix (`jpg/jpeg/png/gif/tif/tiff/bmp/webp`) as a "sibling image", but this Qt build's image engine can decode only `jpg/png/gif/bmp/ico` natively (trimmed `qtbase` image plugins: gif/ico/jpeg + built-in png/bmp). TIFF and WEBP are not decodable by `ImageEngine`. Investigation established:

- **TIFF is fully supported as a multi-page document via MuPDF** — MuPDF's image-document handler (`img_extensions` includes `tif/tiff`) counts every TIFF IFD as a page (`fz_load_tiff_subimage_count`), so multi-page TIFF is a real paged document, not first-page-only.
- **WEBP has no decoder at all** in this build: MuPDF has no `load-webp`, and the vendored Qt has no webp image plugin. WEBP cannot be opened by any engine we have.

The image folder should contain only formats `ImageEngine` actually decodes; TIFF belongs with the document formats; WEBP is unsupported until a decoder is added to the build.

## What Changes

- The native image set shrinks to `jpg/jpeg/png/gif/bmp/ico`. TIFF/WEBP are removed from `ImageEngine` and `ImageFolder`.
- **TIFF** falls through to the default MuPDF engine and opens as a document: a multi-page TIFF is a paged document (one page per IFD). TIFF is excluded from the image-folder sibling set, like PDF/CBR/CB7.
- **WEBP** is dropped (not routed anywhere) — no decoder exists; support is tracked as a follow-up (adding the Qt webp plugin + `libwebp` to the overlay port).
- Remove the now-unreachable raster→MuPDF fallback in `openDocument`/`openSibling`.
- The image folder becomes homogeneous and honest: every listed sibling is a single-page, natively-decoded image.

## Capabilities

### New Capabilities
- `tiff-document-support`: Treat TIFF as a document opened through the MuPDF engine — multi-page paged doc (one page per IFD), excluded from the image folder, grouped with PDF/CBR/CB7.

### Modified Capabilities
- `image-folder-browsing`: The sibling image set uses only natively-decodable raster formats (`jpg/jpeg/png/gif/bmp/ico`); TIFF is no longer counted or browsed as an image.
- `image-engine`: TIFF is no longer routed to or handled by `ImageEngine`; the MuPDF fallback for these is removed.

## Impact

- `src/formatdispatcher.cpp` — `isRasterSuffix` drops `tif/tiff/webp`; TIFF falls through to the default MuPDF engine; WEBP is not routed.
- `src/imagefolder.cpp` — `isRasterName` drops `tif/tiff/webp`.
- `src/viewercontroller.cpp` — remove the raster-fallback branch in `openDocument`/`openSibling` (no longer reachable); `isRasterPath` updated.
- `src/imageengine.cpp` — comment/format-trim only (TIFF/WEBP never reach it).
- `tools/generate_sample_images.py`, `examples/` — add `sample-multipage.tiff` (3 IFDs); remove `sample4.webp` because webp has no decoder.
- No new dependency or public API change.