## 1. Shared infrastructure

- [ ] 1.1 Create `src/naturalsort.h`: move `naturalCompare()` from `comicengine.cpp:27-60` into a header-only inline function (same body, lower-cased code-point + digit-run comparison).
- [ ] 1.2 Update `comicengine.cpp` to include `naturalsort.h` and drop its anonymous-namespace copy; confirm comic entry ordering is unchanged.
- [ ] 1.3 Create `src/imagefolder.h`/`imagefolder.cpp`: an `ImageFolder` class with a `scan(const QString& filePath)` method that lists the directory via `QDir::entryList`, filters to the raster-image suffixes (`.jpg`, `.jpeg`, `.png`, `.gif`, `.tif`, `.tiff`, `.bmp`, `.webp`, case-insensitive), sorts naturally via `naturalsort.h`, and records the current file's index.
- [ ] 1.4 Add the accessors to `ImageFolder`: `hasPrevSibling()`, `hasNextSibling()`, `prevSiblingPath()`, `nextSiblingPath()`, and `isImageFile(path)` (used for classification). Non-image paths or unreadable directories yield an empty (no-sibling) folder.
- [ ] 1.5 Add `imagefolder.cpp` to the `CMakeLists.txt` source list for all targets; both the Windows and Linux presets compile.

## 2. Controller integration (shared)

- [ ] 2.1 In `ViewerController`, add an `ImageFolder m_folder` member and scan it from `openDocument(path)` (viewercontroller.cpp:49) before opening the engine; skip the scan when `isImageFile(path)` is false.
- [ ] 2.2 Extend `openDocument` with an optional `landingPage` parameter (default 1): after the existing reset (`setPageCount`, `resetPage`, cache clear), set the current page to `landingPage` when it is in `[1, pageCount]`.
- [ ] 2.3 Add `bool openSibling(int direction)` (+1 next, -1 prev) to the controller: it resolves the sibling path from `ImageFolder`, calls `openDocument(path, direction > 0 ? 1 : pageCountBackfill)` — for `prev` it opens then lands on the last page — and returns `!m_folder->isEmpty()` result.
- [ ] 2.4 Hook paged-mode boundary spanning into `nextPage()` and `prevPage()` (viewercontroller.cpp:95-113): when the `ViewerState` advance returns `false` and `m_folder->hasNextSibling()` / `hasPrevSibling()` is true, call `openSibling(+1/-1)`, scroll the view to the landing page top, and return `true`.
- [ ] 2.5 Keep `firstPage()`, `lastPage()`, and `goToPage()` clamped (no sibling spanning for jump commands).

## 3. Toolbar integration (shared)

- [ ] 3.1 Add `hasNextSibling()`/`hasPrevSibling()` passthroughs to `ViewerController` (delegating to `m_folder`; false when no document).
- [ ] 3.2 Update `ToolbarPresenter::refreshState()` (toolbar.cpp:16-17): enable `PrevPage` when `hasDoc && (page > 1 || hasPrevSibling())` and `NextPage` when `hasDoc && (page < count || hasNextSibling())`.
- [ ] 3.3 Update the continuous-mode paths `onPrevPage()`/`onNextPage()` (toolbar.cpp:88-114): when `base` is at the boundary and a sibling exists in that direction, call `m_controller->openSibling(±1)` and scroll to the new document top instead of clamping.

## 4. Build and verification

- [ ] 4.1 Build the Windows preset (`cmake --preset windows-x64-release && cmake --build --preset windows-release`); confirm clean compile.
- [ ] 4.2 Build the Linux preset (`cmake --preset linux-release && cmake --build --preset linux-release`); confirm clean compile.
- [ ] 4.3 Smoke test (Windows): open a JPEG in a folder with several mixed-name images (`img1`, `img2`, `img10`); confirm Next is enabled and opens `img2`, Prev from `img2` returns to `img1`, and the last image's Next is disabled with the page indicator showing the new file at `1 / 1`.
- [ ] 4.4 Smoke test (Windows): open a PDF in the same folder; confirm Prev/Next clamp at bounds (no sibling spanning) and the buttons disable at page 1 / last page.
- [ ] 4.5 Smoke test (Linux): repeat 4.3 and 4.4 on the Qt viewer (toolbar buttons, continuous and paged modes).
- [ ] 4.6 Regression: comic CBR/CB7 navigation order is unchanged after the `naturalCompare` extraction.