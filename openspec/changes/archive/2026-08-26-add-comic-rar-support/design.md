## Context

`SUPPORTED_EXTENSIONS` advertises CBR and CB7, but MuPDF (our only archive renderer today) reads neither RAR nor 7z; only ZIP-based files mislabeled `.cbr` worked. SumatraPDF-class apps solve this with the proprietary unrar library, whose license restricts usage to RAR decompression and is hostile to static linking hygiene.

## Goals / Non-Goals

**Goals**
- Honest CBR (RAR4/RAR5) and CB7 support through one dependency.
- Same viewer surface as other engines: zoom/DPI/rotation, paged + continuous.
- Deterministic page order that matches how comics are actually numbered.

**Non-Goals**
- Text layers/outline for image-only comics (none exist).
- Writing/creating archives (read-only).
- Replacing MuPDF's fast CBZ path.

## Decisions

### Decision 1: libarchive over unrar

libarchive is BSD-licensed and reads RAR4, RAR5, 7z, ZIP, and TAR read-only. One dependency repairs both advertised extensions without licensing friction.

- *Alternatives:* unrar (license restrictions), Minizip-based RAR hacks (nonexistent), shelling out to `unrar.exe` (external binary dependency). 
- *Effect:* `libarchive` joins vcpkg.json (Windows) and the Linux package list; located via `find_path`/`find_library` into an imported target on both platforms (the vcpkg port ships a find-package wrapper, not a clean imported-target config).

### Decision 2: Qt decodes the pages, not MuPDF

Comic pages are plain images. Extracting entry bytes to memory and decoding via `QImage::fromData` keeps the engine self-contained (MuPDF never sees the archive) and reuses the statically linked Qt Gui image loaders (PNG/JPEG/GIF/BMP are enabled in our qtbase features).

- *Alternatives:* feeding extracted images to MuPDF as a synthetic document — rejected: extra indirection for zero gain.
- *Effect:* rendering honors zoom (`scaled`, smooth) and rotation (`QTransform`) mirroring `MuPdfEngine`'s semantics; `pageDimensions` caches a zoom-1 render like `ChmEngine`.

### Decision 3: Natural-order page sort

Digit runs in entry names compare numerically (case-insensitive), so `page2.jpg` precedes `page10.jpg`. Directory entries and non-image extensions are skipped during enumeration.

- *Effect:* stable page numbering matching user expectation; no reliance on archive entry order.

## Risks / Trade-offs

- **[Risk] RAR5 edge cases** (encrypted headers, multi-volume sets). *Mitigation:* libarchive reports failure → open fails cleanly per spec; multi-volume unsupported (single-file archives are the norm for comics).
- **[Risk] Qt static build lacks TIFF/WebP plugins**, so those image types inside CBRs won't decode. *Mitigation:* documented filter list; CBZ (the common format for exotic codecs) stays on MuPDF which handles them.
- **[Risk] Large archives load one page at a time into memory** — bounded by single image size, same as every other engine's render cache.

## Migration Plan

1. Wire libarchive + `ComicEngine`; route `cbr`/`cb7`.
2. Extend the smoke harness sweep to cover the new engine via dispatcher (zip-CBR exercises the pipeline; real RAR verified manually).
3. Host verification with a genuine RAR comic from the user's collection.

## Open Questions

None.
