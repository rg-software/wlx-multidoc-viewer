## Context

Standalone raster images today fall through to `MuPdfEngine` (see `src/formatdispatcher.cpp:28`), which neither decodes GIF frame timing nor knows about sibling files; multi-frame GIFs show only their first frame and multi-frame TIFFs surface as step-through pages. The codebase already has the two precedents this change builds on: `ComicEngine` decodes images through Qt and sorts entries with an in-file `naturalCompare` (`src/comicengine.cpp:27`), and `ViewerController` owns all navigation (`nextPage`/`prevPage`/`hasNextPage`/`hasPrevPage`, `src/viewercontroller.cpp:108-215`) while thin per-platform viewers install OS-native timers and call back into it. See proposal.md – Why/What for motivation and scope.

## Goals / Non-Goals

**Goals:**
- Add a Qt `QImageReader`-backed `ImageEngine` that treats every raster as one page and reports animation metadata (`src/imageengine.*`).
- Animate GIFs in place with a controller-driven playback pump and a single narrow per-frame repaint.
- Extend next/prev so at a standalone-image document's boundary the adjacent sibling in the same directory opens instead of clamping.
- Reuse natural sort as a shared helper so comic pages and image siblings order identically.
- Zero regression: any raster file the image engine can't decode falls back to MuPDF.

**Non-Goals:**
- No new toolbar buttons — Total Commander/Double Commander already supply navigation; this only re-enables existing Prev/Next at bounds.
- No full-screen slideshow, autoplay, or folder-map UI.
- No animation for non-GIF formats (multi-frame TIFFs remain a single composite of their first frame, as today via the decode path).
- No changes to the DjVu or CHM engines.

## Decisions

### D1. `ImageEngine` decodes through `QImageReader`, not `QImage::fromData`

`QImageReader` exposes the animation surface the GIF feature needs: `imageCount()`, `jumpToImage(n)`, `currentImageNumber()`, `nextImageDelay()` (ms), and `loopCount()`. `QImage::fromData` (what `ComicEngine` uses) shows only the first frame and is unusable for animation. This mirrors the comic precedent of Qt-provided decoding while adding the frame-timing metadata.

- Alternatives considered: decoding via MuPDF's image path (rejected — no frame timing), parsing GIF internals ourselves (rejected — reimplementing Qt's decoder, no reason to), the deprecated `QMovie`. `QImageReader` is the current supported API.

### D2. Animation is surfaced as no-op default virtuals on `DocumentEngine`

Add `isAnimated()`, `frameDelayMs()`, and `advanceFrame()` to `DocumentEngine` (`src/document.h`) with no-op defaults (`false`/`0`/no-op), so every other engine keeps working unchanged; only `ImageEngine` overrides them. No new interface type or dynamic-cast in the controller.

- Alternatives: a `QImageReader`-typed cast check in the controller (rejected — couples shared logic to one backend; the virtual-default approach keeps `ViewerController` engine-agnostic and matches the existing pattern of `hasSelectableText`/`supportsSearch` no-op defaults).

### D3. Natural sort is extracted to a shared `naturalsort.h`

Copy the `naturalCompare` body out of `comicengine.cpp:27` into `src/naturalsort.h` as a `naturalCompare(const QString&, const QString&)` function; `ComicEngine` and the new `ImageFolder` both call it. Byte-for-byte same comparator so sibling order and comic page order are identical.

### D4. Sibling discovery lives in `src/imagefolder.*`, not the controller

`ImageFolder` lists the directory entries at load, filters to the raster suffixes in the proposal (`jpg/jpeg/png/gif/tif/tiff/bmp/webp`), sorts naturally, and yields prev/next sibling for a given file. `ViewerController` holds an `ImageFolder` instance built from the open path only when the active engine is an image document (decided by `isAnimated()`/a small `isStandaloneImage` capability flag), and uses it to decide boundary behavior.

- Alternatives: teaching the controller to scan directories directly (rejected — pollutes its single-file focus and duplicates the filter); a separate navigation service (rejected — overkill for one feature).

### D5. Boundary next/prev in the controller opens the sibling via a re-open

When `nextPage()`/`prevPage()` would clamp (existing early `return false`) AND an image sibling exists on that side, the controller opens the sibling path in place (stop animation, `createEngine`-equivalent open of the sibling, reset state) and returns `true`. `hasNextPage()`/`hasPrevPage()` likewise return sibling-existence at the bound so the toolbar can enable Prev/Next there.

### D6. Playback pump is controller-driven, timer is platform-owned

`ViewerController` adds an animation pump: after any layout/repaint it reports the next frame delay (`frameDelayMs()` of the current frame) via `startAnimationDelayMs()`, and the platform viewer arms its own timer — `SetTimer`/`WM_TIMER` on Win32 (`src/viewer_win32.cpp`), a single-shot `QTimer` on Linux (`src/viewer.cpp`). On tick the viewer calls `animationTick()`, which calls the engine's `advanceFrame()`, bumps a frame-invalidation counter, and triggers a narrow repaint; the controller then reports the new delay to re-arm. The pump stops on close/file-switch/window-destroy.

- Alternatives: a worker thread ticking frames (rejected — a background render worker was already tried and reverted for the same reason: thread-safety/FIFO ordering vs. UI-thread simplicity, see the abandoned `async-render-worker`); controller-owned `QTimer` on the Win32 path (rejected — Win32 has no event loop, so the platform must own the timer anyway).

### D7. Frame repaint is a narrow invalidation, not a full `relayout`

Advancing a frame re-renders only the current page's bitmap (dimensions are unchanged, so layout, scroll, and the virtual-canvas rects are untouched). The controller exposes a frame counter the viewer keys off to repaint the visible page region without relayout.

## Risks / Trade-offs

- **R1: Trimmed Qt build may omit the GIF image plugin** (static plugin list could drop `qgif`) → Verify the host's Qt6 ships the GIF handler; if absent, add it to the Qt feature set. Mitigated by the MuPDF fallback (D: a GIF that fails to load still opens, just unanimated).
- **R2: Folder with many large images scans the whole directory at open** → `ImageFolder` lists entries metadata-only (no decode) at load; cost is proportional to directory size, acceptable for lister use. `pageDimensions` still lazily decodes only on demand.
- **R3: Loop-count arithmetic edge cases** (a GIF declaring zero/infinite loops, a partial last loop) → `ImageEngine` clamps: treat zero/infinite as "loop forever", honor a finite positive count exactly; `advanceFrame` returns whether playback should continue so the pump can stop cleanly.
- **R4: Sibling order could surprise** (natural order vs. sloppy file naming) → Same deterministic comparator already accepted for comic archives (proposal mandates sharing it); behavior is explicit and testable.
- **R5: Behavioral BREAKING change** — animated GIFs are no longer multi-page. → Confirmed acceptable in the proposal; the viewer's page indicator and toolbar reflect a single-page image document.

## Platform-specific code

| Concern | Windows (`viewer_win32.cpp`) | Linux (`viewer.cpp`) |
|---|---|---|
| Playback timer | `SetTimer` on the viewport HWND, handled in `WM_TIMER`; `KillTimer` on close/destroy | single-shot `QTimer` re-armed per tick; `.stop()` on close/reload |
| Frame repaint | Narrow `InvalidateRect` of the page area + `BitBlt` | `update()`/narrow repaint of the page widget |
| Re-open on boundary | Reuse existing `ListLoad` open path (create engine, `openDocument`) with the sibling path | Same, via the Qt open path |
| Timer teardown | In `ListCloseWindow`/destroy handler | In widget destructor / close handler |

The controller is shared and platform-agnostic: all animation arithmetic, sibling resolution, and repaint scope decisions live in `ViewerController`; the platform layers only arm a timer and repaint a region.

## Migration Plan

Single feature; no data migration or config change. Rollback is a revert of the routing in `src/formatdispatcher.cpp` (back to MuPDF fallback) — no behavioral API break. Commit sequence follows the conventional-commit convention (feat: with a scope, e.g. `feat(images):`), landing the shared helper (D3) first, then the engine, then folder browsing, then animation.