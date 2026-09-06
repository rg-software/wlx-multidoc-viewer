## Why

A lister window shows a single image at a time, but Total Commander / Double Commander users typically have folders full of images. Today the only way to move to the adjacent image is to close the lister and open the next file; there is no way to "flip through" a folder's images inside the lister. GIFs currently expose each frame as a page, so the folder-browsing feature must treat multi-frame image files as single items, not as multi-page documents.

## What Changes

- Detect the sibling image files in the current file's directory at load time and expose the adjacent previous/next files.
- Extend the existing next/prev navigation so that at the boundary of a single-file image the viewer opens the neighboring sibling image instead of clamping in place. This applies when the open document is a standalone image (single-page document opened through the Mupdf/fallback path) and there is a sibling in the same folder.
- Enable the toolbar Prev/Next buttons at document boundaries when a sibling image is available, and keep the in-document page indicator consistent with the newly opened image.
- Reuse the natural filename ordering already used by `comicengine.cpp` for deterministic next/prev order.
- Add a dedicated code path on both Windows and Linux; behavior must be identical on both platforms.

## Capabilities

### New Capabilities
- `image-folder-browsing`: Cross-file navigation between sibling images in the same directory, including sibling detection, boundary-spanning next/prev commands, toolbar enablement, and ordering rules.

### Modified Capabilities
- `viewer-navigation`: The "next/prev at document bounds" behavior changes from *always clamp in place* to *clamp unless the active document is a standalone single-page image with a sibling in the same folder, in which case the sibling is opened*. This is a delta in `openspec/specs/viewer-navigation/spec.md`.

## Impact

- `src/formatdispatcher.cpp` — feature detection: standalone image vs. multi-page document.
- New sibling-discovery module (directory scan + natural sort; reuses `comicengine.cpp`'s sort helper).
- `src/viewercontroller.cpp/.h` — folder context, sibling-aware next/prev, document hot-swap via existing `openDocument`.
- `src/toolbar.cpp` — `refreshState` enables Prev/Next at boundaries when a sibling exists.
- `src/plugin.cpp` — passes the file directory context into the viewer.
- Windows and Linux viewer backends are unaffected structurally (both route through the controller).