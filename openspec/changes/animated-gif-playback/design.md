## Context

See `proposal.md` — Why/What. Current state relevant to the design:

- `.gif` files route through `formatdispatcher.cpp:28` to `MuPdfEngine`; MuPDF decodes a GIF as a multi-frame document, so `pageCount()` is the frame count and each frame is a static "page". MuPDF does not expose per-frame delays, so frame-stepping is the only possible behavior on that path.
- `DocumentEngine` (document.h:50-83) is a small pure interface; it already carries default-implemented virtuals for optional capabilities (`hasSelectableText`, `pageText`, `supportsSearch`, `searchText`), so optional animation virtuals follow the same established pattern.
- The Windows viewer is a pure Win32 window using `SetTimer`/`WM_TIMER` (no Qt event loop); the Linux viewer has a full Qt event loop and `QTimer`.
- Windows links Qt6 Core+Gui statically. `QImageReader` lives in QtGui and needs no `QApplication`; GIF decoding is provided by Qt's built-in gif image handler (see Risks — the trimmed build must still ship it).

## Goals / Non-Goals

**Goals:**
- A dedicated GIF engine that decodes frames and per-frame delays and never regresses to frame-stepping pages.
- A playback pump in the shared controller driven by a per-platform timer, re-armed from each frame's own delay.
- Correct loop semantics and clean teardown on Windows and Linux.
- Zero changes to the other engines, the toolbar contract, or the print pipeline interface.

**Non-Goals:**
- Play/pause UI controls, scrubber, or frame list (issue asks only to "show animation if possible").
- Animated WebP/AVIF or other animated formats (out of scope; engine is gif-only).
- Correcting MuPDF's static frame behavior for GIFs in other engines.
- Threaded frame decode (frames are decoded on the UI timer tick; see Risks).

## Decisions

**D1 — Route `.gif` to a new Qt `QImageReader`-backed `GifEngine`; fall back to MuPDF if it cannot open the file.**
`QImageReader` exposes everything needed: sequential `read()`, `jumpToImage()`/`jumpToNextImage()`, `nextImageDelay()`, `frameCount()`, and `loopCount()` (0 = infinite, -1 = no loop, N = finite). `formatdispatcher` attempts `GifEngine::open()` for `.gif` and falls back to the MuPDF path if it fails, so exotic GIFs today's MuPDF path opens still open.
*Alternatives rejected:* timing via MuPDF frame pages (no delay metadata on `fz_page` — the whole reason a dedicated engine is needed); a general "animated image" engine (scope creep; gif only per the issue).

**D2 — Animation virtuals on `DocumentEngine` with no-op defaults.**
Add `virtual bool isAnimated() const { return false; }`, `virtual int frameDelayMs() const { return 0; }`, and `virtual bool advanceFrame() { return false; }`. `GifEngine` overrides them; every other engine inherits the defaults unchanged. This mirrors the existing `supportsSearch` precedent and avoids `dynamic_cast` or a second interface.
*Alternatives rejected:* a separate `AnimatedImageEngine` interface with `dynamic_cast` (more ceremony for a single implementor); hardcoding gif detection in the controller via suffix checks (the engine is the right owner of its own capability).

**D3 — `GifEngine` keeps one open `QImageReader`, one cached unscaled frame, and a mutex for the render path.**
`open()` verifies `QImageReader::supportedImageFormats()` contains `gif`, sets `setAutoTransform(true)` (EXIF-aware), and decodes frame 0. `renderPage(page, zoom, dpi, rotation)` scales the *current cached frame* (Qt smooth transform) to `zoom × dpi` and applies rotation, all under a mutex, so the print worker's `renderPage` at print resolution always sees a consistent frame. `advanceFrame()` (UI thread) advances the reader, checks `loopCount()` iteration arithmetic, and either wraps via `jumpToImage(0)` (infinite loops) or reaches the terminal state (finite loops exhausted → returns 0, stays on last frame).
*Alternatives rejected:* re-decoding on every `renderPage` (wasteful — frame content changes only on advance); pushing decode onto the print worker (the worker must not mutate the UI-thread reader).

**D4 — Playback pump in `ViewerController`, timer owned by the viewer.**
The controller exposes `int startAnimationDelayMs()` (initial arm), `int animationTick()` (advance + return next delay, 0 = stop), `bool animationActive()`, and `void stopAnimation()`. `animationTick()` calls `GifEngine::advanceFrame()`, clears the page-1 cache entry so the next paint re-renders the new frame, and returns the next delay. The viewer owns the timer because Windows has no Qt event loop: Win32 uses `SetTimer` + `WM_TIMER` re-armed each tick; Qt uses a single-shot `QTimer` restarted each tick. Both call the same `animationTick()`.
*Alternatives rejected:* the engine owning a `QTimer` (won't fire on the Win32-only build); a long-lived repeating timer at a fixed interval (ignores per-frame delays).

**D5 — Frame changes repaint via a narrow invalidation path.**
The viewer's timer callback calls `animationTick()`, then invalidates only the viewport region (Win32 `InvalidateRect` / Qt `update()`); the existing paint path calls `renderCachedViewport`, which re-renders the fresh frame. The fully-general `notifyChanged()` (toolbar refresh, scroll anchoring, selection) is deliberately NOT used per tick.
*Alternatives rejected:* routing each tick through `notifyChanged()` (would churn viewer state and risk scroll/selection side effects for something that is purely a repaint).

**D6 — Static GIFs never start a timer.**
`GifEngine` reports `isAnimated() == false` when `frameCount() <= 1`; the viewer only arms the timer when `animationActive()` is true after open. A static GIF renders frame 0 once.

**D7 — Playback lifecycle is bound to document open/close on the viewer side.**
The viewer arms the timer after a successful `openDocument` when the new engine is animated, and tears it down (`KillTimer` / timer stop) in its document-close, file-switch, and window-destroy paths — before `closeDocument()`/engine teardown runs. This guarantees no timer fires against a torn-down engine.
*Alternatives rejected:* the controller killing the timer (it does not own the platform timer handle).

## Platform-Specific Code

- **Shared:** `gifengine.*` (decoding + timing + loop arithmetic, Qt-only but platform-agnostic), `formatdispatcher.cpp` routing, controller playback pump and animation virtuals.
- **Windows (`viewer_win32.cpp`):** `SetTimer(hwnd, kGifTimerId, startAnimationDelayMs(), …)`, `WM_TIMER` handler → `controller->animationTick()` → re-arm or `KillTimer`, `InvalidateRect`. Teardown in `ListCloseWindow` and the file-swap path.
- **Linux (`viewer.cpp`):** single-shot `QTimer` member, restarted with the returned delay each tick, `update()` on the canvas. Teardown in document close / file-swap / widget destructor.
- **Untouched:** `mupdfengine.*`, `djvuengine.*`, `chmengine.*`, `comicengine.*`, `toolbar.*`, `print_*`, `searchcontroller.*` — all inherit the no-op animation defaults.

## Risks / Trade-offs

- [Trimmed static Qt build may exclude the GIF image handler] → `GifEngine::open()` refuses when `supportedImageFormats()` lacks `gif`; `formatdispatcher` then falls back to today's MuPDF frame behavior (no regression). If that happens, fix the trimmed Qt config (keep the gif plugin) as a follow-up.
- [`QImageReader` decode without a `QApplication` on Windows] → `QImageReader` is platform-integration-free and the project already uses `QImage` standalone; verify in the first smoke test and fall back to MuPDF per D1 if anything unexpectedly requires a QGuiApplication.
- [Scaling a full-res frame on every tick can overrun short delays (e.g. 50 ms) on large GIFs] → Frames decode/scale on the UI thread; a re-arm uses the declared delay regardless. GIF frames are typically small; if a pathological GIF stalls the UI, a pre-scaled-cache optimization is a follow-up, not a spec change.
- [Finite-loop GIFs out of range of `jumpToImage`] → For non-restartable wraparound, recreate the `QImageReader` on the same file (cheap relative to a frame decode); `loopCount()` math is in the engine, so both platforms behave identically.
- [Coordination with `image-folder-browsing`] → In both orders the GIF is classified by suffix as an image; with animation it is one page, so folder spanning still occurs at that single page. No conflict between the two changes.

## Migration Plan

No data migration. Behavior changes only for `.gif`: previously frame-stepped pages become one animated page (the intended fix for issue #5, part 2). Deploy in one commit; rollback reverts routing (`.gif` back to MuPDF) without touching persisted state.

## Open Questions

None that would change the specs, approach, or task breakdown. (GIF-handler availability in the trimmed Qt build is verified at task time with a defined fallback, not a decision that changes the spec.)