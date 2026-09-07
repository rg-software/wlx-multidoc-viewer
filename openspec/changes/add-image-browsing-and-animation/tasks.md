## 1. Shared Infrastructure

- [ ] 1.1 Extract `naturalCompare` from `comicengine.cpp:27` into `src/naturalsort.h` as a shared function; keep `ComicEngine` using it via the shared header
- [ ] 1.2 Add no-op default animation virtuals to `DocumentEngine` in `src/document.h`: `isAnimated()` (false), `frameDelayMs()` (0), `advanceFrame()` (no-op)

## 2. Image Engine

- [ ] 2.1 Create `src/imageengine.h`/`src/imageengine.cpp`: a `DocumentEngine` that opens a raster file via `QImageReader` and exposes a single page
- [ ] 2.2 Implement `renderPage`/`pageDimensions` to decode the current `QImageReader` image honoring zoom, DPI scale, and rotation (mirroring `ComicEngine::renderPageLocked`)
- [ ] 2.3 Implement animation virtuals: report `isAnimated()` (frame count > 1 and loop not static), `frameDelayMs()` from `nextImageDelay()`, and `advanceFrame()` via `jumpToImage`/loop arithmetic; clamp zero/infinite loops to "loop forever"
- [ ] 2.4 Implement `open()` so that a file `QImageReader` cannot decode returns `false` (triggering MuPDF fallback); `extractText`/`outline`/`metadata` return empty defaults

## 3. Format Routing

- [ ] 3.1 Route raster suffixes (`.jpg/.jpeg/.png/.gif/.tif/.tiff/.bmp/.webp`, case-insensitive) from MuPDF to `ImageEngine` in `src/formatdispatcher.cpp`, keeping MuPDF as the fallback whenever `ImageEngine::open` fails
- [ ] 3.2 Verify the trimmed Qt static build for the host ships the GIF image handler (R1), and confirm an undecodable raster still falls back to MuPDF with no regression

## 4. Folder Browsing

- [ ] 4.1 Create `src/imagefolder.h`/`src/imagefolder.cpp`: list sibling raster images in a file's directory at load, filter to the proposal's raster suffixes, sort with shared `naturalCompare`
- [ ] 4.2 Add sibling resolution to `ViewerController` (`src/viewercontroller.cpp/.h`): build an `ImageFolder` from the open path when the engine is a standalone image document, exposing prev/next sibling for the current file
- [ ] 4.3 Modify `nextPage`/`prevPage` (and continuous-mode variants) so a boundary-stop re-opens the sibling image path when one exists instead of clamping and returning `false`
- [ ] 4.4 Update `hasNextPage`/`hasPrevPage` to return sibling-existence at the document bound so the toolbar can enable Prev/Next there (`src/toolbar.cpp` enablement consumes these)

## 5. Animation Playback (shared controller)

- [ ] 5.1 Add the animation pump to `ViewerController`: `startAnimationDelayMs()` to report the current frame delay, `animationTick()` to advance the frame and flag a narrow repaint, and pump teardown in `closeDocument`/`openDocument`
- [ ] 5.2 Expose a frame/invalidation counter so platform viewers can repaint the visible page region without a full `relayout`
- [ ] 5.3 Stop playback on file switch, close, or window destruction (all pump teardown paths)

## 6. Win32 Viewer Integration

- [ ] 6.1 In `src/viewer_win32.cpp`, arm the playback timer with `SetTimer`/`WM_TIMER` when the controller reports a delay, and `KillTimer` on close/destroy
- [ ] 6.2 On timer tick call `animationTick()` and repaint the narrowed page area via `InvalidateRect`/`BitBlt`
- [ ] 6.3 Wire boundary sibling re-open through the Win32 open path (ListLoad/`openDocument`)

## 7. Linux Viewer Integration

- [ ] 7.1 In `src/viewer.cpp`, arm a single-shot `QTimer` re-armed per tick when the controller reports a delay, and stop it on close/reload
- [ ] 7.2 On tick call `animationTick()` and repaint the narrowed page region; stop the timer in the widget destructor/close handler
- [ ] 7.3 Wire boundary sibling re-open through the Qt open path and re-verify scroll + selection + toolbar behavior in Total/Double Commander