## Why

Continuous mode builds one giant "strip" bitmap capped at 1.5M px, lays pages out with a uniform stride based on the current page's size, and re-renders synchronously on every scroll step. This produces three user-visible defects on both platforms: fast scrollbar navigation lands on an earlier page than requested, scrolling is sluggish, and mixed-size documents cause erratic jumps because the strip geometry and fit zoom keep shifting as the current page changes.

## What Changes

- Replace the single capped strip with a per-page pixel layout in `ViewerController`: the controller owns a virtual canvas (exact per-page offsets + dimensions), so `pageAtScrollOffset()` / scrollbar range cover the real document height and mixed page sizes align correctly.
- Paint each visible page from a small per-page render cache instead of re-building one tall bitmap; pages enter/leave the cache as they scroll in/out of view, so scroll cost is bounded to the viewport instead of the whole document.
- Keep fit-mode zoom anchored: `recomputeFitZoom()` and continuous-mode layout changes no longer recompute against the current page alone, removing the viewport-resize wobble on rotation/zoom/page change.
- On the Win32 viewer, replace the single `HBITMAP` strip paint with per-page `HBITMAP` blits; the Qt viewer keeps its `QPixmap` strip but sources it from the new layout/cache API and shares the same scroll-range math.
- Explicitly drop the `kMaxStripHeight` memory cap for the scrollbar range (the virtual canvas is bounded by total document height, not a bitmap), keeping the memory bound on the per-page cache instead.

## Capabilities

### New Capabilities
- `viewer-scrolling`: continuous-mode virtual canvas layout, per-page rendering cache, scrollbar range computed from real document geometry, and viewport-anchored zoom recompute. Applies to both the Win32 and Qt viewers.

### Modified Capabilities
<!-- None: no existing specs live under openspec/specs/ yet (only in-progress change deltas).
     The viewer-baseline delta spec viewer-rendering currently describes a single strip;
     this change supersedes that requirement when archived. -->

## Impact

- `src/viewercontroller.h/.cpp` — replace strip compose with layout + cache; new scroll/range/anchor API.
- `src/viewer_win32.h/.cpp` — per-page paint, scrollbar range from controller, keyboard/wheel unchanged.
- `src/viewer.h/.cpp` — switch to per-page renderer API; keep continuous scroll behavior via shared range math.
- `src/viewer_settings.h` — remove/repurpose `kMaxStripHeight`; tuned cache size constants.
- No changes to `DocumentEngine` or the engines (MuPDF/DjVu) — rendering stays synchronous at the engine layer; async rendering is tracked separately.