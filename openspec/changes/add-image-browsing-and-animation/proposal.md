## Why

Standalone raster images (JPEG/PNG/GIF/TIFF/BMP/WEBP) currently open through the MuPDF fallback, which has no animation support: animated GIFs show only their first frame, and multi-frame TIFFs expose frames as pages. Viewers also cannot flip through a folder of images — reaching the next image requires closing the lister and opening another file. Both gaps share one root cause: images are served by a document engine that neither decodes frame timing nor the file's neighborhood.

## What Changes

- Route the raster-image suffix list (`.jpg/.jpeg/.png/.gif/.tif/.tiff/.bmp/.webp`) from MuPDF to a new `ImageEngine` that decodes through Qt `QImageReader`, mirroring the existing `ComicEngine` precedent of Qt image decoding.
- Treat every raster file as a single document page: a multi-frame GIF or TIFF is one page whose content animates in place, never a set of step-through pages.
- Animate GIFs in the lister: per-frame timing from the file's declared delays, honoring the file's own loop count; playback stops on close, file switch, or window destruction. Static GIFs render once.
- Detect sibling images in the open file's directory at load time and extend next/prev so that at an image document's boundary the adjacent sibling image opens instead of clamping (natural filename order, shared with comic archives).
- Keep MuPDF as fallback when `ImageEngine` cannot decode a raster file, so no currently-openable file regresses. Behavior is identical on Windows and Linux.
- **BREAKING** (behavioral, not API): GIFs are no longer a multi-page document; folder browsing reaches siblings only at the single page's boundary.

## Capabilities

### New Capabilities
- `image-engine`: Dedicated Qt `QImageReader`-backed engine for standalone raster images — decoding, single-page treatment of multi-frame files, animation metadata, and MuPDF fallback on decode failure.
- `gif-animation`: In-place playback of animated GIFs in the lister — per-frame timing, loop semantics, single-page navigation treatment, and playback lifecycle tied to document open/close.
- `image-folder-browsing`: Cross-file navigation between sibling images in the same directory — sibling detection at load, boundary-spanning next/prev commands, deterministic natural ordering, and toolbar enablement at document bounds.

### Modified Capabilities
- `viewer-navigation`: The "clamp navigation at document bounds" requirement changes for standalone image documents — next/prev at a boundary opens the sibling image instead of staying put when one exists.

## Impact

- `src/formatdispatcher.cpp` — route raster suffixes to `ImageEngine`; fall back to MuPDF.
- New `src/imageengine.*` — Qt `QImageReader`-backed engine (decode, frame timing, loop arithmetic, animation virtuals).
- New `src/imagefolder.*` + `src/naturalsort.h` — sibling discovery/ordering (extracted from `comicengine.cpp`).
- `src/document.h` — optional animation virtuals with no-op defaults (`isAnimated`, `frameDelayMs`, `advanceFrame`).
- `src/viewercontroller.cpp/.h` — folder context, sibling-aware next/prev, animation pump (`startAnimationDelayMs`/`animationTick`).
- `src/toolbar.cpp` — Prev/Next enablement at document bounds when a sibling exists.
- `src/viewer_win32.cpp` / `src/viewer.cpp` — per-platform playback timer (`SetTimer`/`WM_TIMER` vs single-shot `QTimer`) and narrow per-frame repaint.
- `src/comicengine.cpp` — swap to shared `naturalCompare`.
- Trusted Qt for GIF decode; verify the trimmed static Qt build ships the GIF image handler.