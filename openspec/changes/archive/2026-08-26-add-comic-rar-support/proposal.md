## Why

Real-world CBR comic archives are RAR containers, and MuPDF has no RAR reader — the plugin only opened CBR files that were secretly ZIPs, and CB7 (7-Zip archives) never worked at all. Both extensions are advertised in the detect string, so hosts offer the plugin for files it cannot render.

## What Changes

- Add `libarchive` as a dependency (BSD-licensed; reads RAR4/RAR5, 7z, ZIP, TAR read-only). Windows via the vcpkg manifest; Linux via system `libarchive-dev` / equivalent.
- New `ComicEngine` implementing `DocumentEngine`: enumerates image entries (`png`, `jpg`, `jpeg`, `gif`, `bmp`) from any archive libarchive can read, sorts them in natural page order (numeric-aware, so `page2` precedes `page10`), and decodes each page through Qt's image loader — honoring zoom, DPI scale, and rotation like every other engine.
- Route `.cbr` and `.cb7` to `ComicEngine`; `.cbz` stays on MuPDF's proven ZIP path.
- No text layer or outline for comics (image-only by nature); find/selection stay disabled for these formats.

## Capabilities

### New Capabilities
- `comic-archive-support`: opening RAR-based (CBR) and 7-Zip-based (CB7) comic archives, page enumeration in natural order, and bitmap rendering of each page.

### Modified Capabilities
<!-- None for this change. -->

## Impact

- `src/comicengine.{h,cpp}` — new engine (~200 LoC).
- `vcpkg.json` — add `libarchive`. `CMakeLists.txt` — locate it via `find_path`/`find_library` into an imported target (same pattern as CHMLib; the vcpkg port ships a wrapper rather than a clean config), add sources to all three targets.
- `src/formatdispatcher.cpp` — route `cbr`/`cb7`.
- Detect string unchanged: `CBR`/`CB7` are already registered there and become honest with this change.
