## 1. Dependency wiring

- [x] 1.1 Add `libarchive` to `vcpkg.json`; add a CHMLib-style `find_path(archive.h)`/`find_library(archive)` block creating an imported `LibArchive::LibArchive` target in `CMakeLists.txt`
- [x] 1.2 Add `src/comicengine.cpp` to the main library and both harness targets; link the new target everywhere

## 2. ComicEngine implementation

- [x] 2.1 Declare `ComicEngine : public DocumentEngine` with libarchive handle, sorted entry list, dimension cache, and mutex
- [x] 2.2 Implement `open`: wide-char filename open on Windows (`archive_read_open_filename_w`), enable all filters/formats, enumerate entries filtering image extensions, sort naturally (numeric-aware digit runs, case-insensitive)
- [x] 2.3 Implement entry extraction to `QByteArray` via `archive_read_data_block` loop
- [x] 2.4 Implement `renderPageLocked`: decode via `QImage::fromData`, convert to RGB888, apply zoom×DPI scale (smooth) and center rotation via `QTransform`
- [x] 2.5 Implement `pageDimensions` with a zoom-1 cached render; `close`/destructor free the archive
- [x] 2.6 Leave text/search APIs at their no-capability defaults (image-only content)

## 3. Dispatch

- [x] 3.1 Route `.cbr` and `.cb7` suffixes to `ComicEngine` in `formatdispatcher.cpp`

## 4. Verification

- [x] 4.1 Windows build green; smoke-harness sweep opens a zip-based `.cbr` and `.cbz` through the new engine path with correct page counts and rendered dimensions
- [x] 4.2 Host verification: open a genuine RAR-based `.cb7`/`.cbr` from the user's collection (navigation order matches natural page numbering)
