## Why

Issue #5, part 2: when a GIF is opened in the lister, only the first frame is shown. MuPDF decodes a GIF as a multi-page document (one page per frame), so users currently step through frames by hand with the page buttons and never see the animation.

## What Changes

- Route `.gif` files to a new `GifEngine` that decodes frames with Qt `QImageReader`, which provides per-frame image data and the native per-frame delay (`nextImageDelay()`).
- Treat an animated GIF as a single document page that animates in place: the viewer advances the frame on a timer (Win32 `SetTimer` on Windows, `QTimer` on Linux), repainting in place.
- Respect the GIF's own loop semantics (`loopCount()`): play once, loop N times, or loop forever, as the file declares.
- A GIF still behaves like one page for navigation, page indicator, zoom/fit, rotate, print, and (once `image-folder-browsing` lands) folder browsing. Static GIFs render one frame and do not animate.
- Playback stops when the document closes, a different file is loaded, or the viewer window is destroyed; the timer is re-armed per frame from the frame's declared delay.
- Both platforms animate identically; the only difference is which timer mechanism the platform viewer uses.

## Capabilities

### New Capabilities
- `gif-animation`: Frame decoding, per-frame timing, looping, single-page treatment of animated GIFs, and playback lifecycle in the lister.

### Modified Capabilities
- (none — the previous behavior of stepping GIF frames as pages was never specified in any capability; all behavior for this feature is new and lives in `gif-animation`.)

## Impact

- `src/formatdispatcher.cpp` — route the `.gif` suffix to the new engine.
- New `src/gifengine.*` — `QImageReader`-backed `DocumentEngine` impl (decode, `frameDelayMs`, frame advance, loop counting).
- `src/document.h` — optional animation virtuals on `DocumentEngine` with no-op defaults (non-breaking for existing engines).
- `src/viewercontroller.cpp/.h` — playback pump: `advanceAnimation()`, animation state, repaint trigger.
- `src/viewer_win32.cpp` — `WM_TIMER` handler re-arming per frame delay.
- `src/viewer.cpp` — `QTimer` on the Qt canvas.
- Windows Qt static link must include the Qt GIF image-format handler (verify at task time; see design risks).