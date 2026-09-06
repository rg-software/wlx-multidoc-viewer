## 1. GifEngine (shared)

- [ ] 1.1 Create `src/gifengine.h`/`gifengine.cpp`: a `GifEngine : DocumentEngine` that opens with `QImageReader` on a `.gif` path, refuses open (returns false) when `QImageReader::supportedImageFormats()` does not include `gif` or the reader fails, sets `setAutoTransform(true)`, and decodes frame 0.
- [ ] 1.2 Implement the base `DocumentEngine` contract on `GifEngine`: `pageCount() == 1`, `renderPage()` scales the current cached frame to `zoom × dpiScale` with smooth transform and applies rotation, `extractText()`/`metadata`/`outline` return empty, and `pageDimensions(1)` returns the frame's natural size. No selectable text, no search.
- [ ] 1.3 Guard the current-frame `QImage` with a `std::mutex` so `renderPage()` (print worker at print resolution) always sees a consistent frame while the UI thread advances it.
- [ ] 1.4 Override the animation virtuals (added in task 2.1): `isAnimated() == (frameCount() > 1)`, `frameDelayMs()` returns the current frame's declared delay (clamped to a reasonable floor, e.g. ≥ 10 ms), `advanceFrame()` advances the reader, applies `loopCount()` loop arithmetic (wrap via `jumpToImage(0)`/reader recreation for infinite loops; stop on last frame for finite/exhausted loops), and returns `true` while animating.
- [ ] 1.5 `close()` releases the `QImageReader`; `open()` on a single-frame GIF reports `isAnimated() == false` and renders frame 0 once.

## 2. Engine interface and dispatch (shared)

- [ ] 2.1 Add no-op default animation virtuals to `DocumentEngine` (document.h): `virtual bool isAnimated() const { return false; }`, `virtual int frameDelayMs() const { return 0; }`, `virtual bool advanceFrame() { return false; }` — other engines unchanged.
- [ ] 2.2 In `formatdispatcher.cpp`, route the `.gif` suffix to `GifEngine`; if `open()` returns false, fall back to the existing `MuPdfEngine` path so previously-openable GIFs keep opening.
- [ ] 2.3 Add `gifengine.cpp` to the `CMakeLists.txt` source list for all targets; both Windows and Linux presets compile.

## 3. Playback pump (shared controller)

- [ ] 3.1 Add controller playback API: `bool animationActive() const`, `int startAnimationDelayMs() const`, `int animationTick()`, `void stopAnimation()` — delegating to `engine->isAnimated()`/`frameDelayMs()`/`advanceFrame()`.
- [ ] 3.2 In `animationTick()`: call `advanceFrame()`; when it returns false or the next delay is 0, stop and return 0; otherwise clear the page-1 render-cache entry so the next paint re-renders the new frame, then return the next delay.
- [ ] 3.3 Ensure `closeDocument()`/`openDocument()` (file switch) clear animation state via `stopAnimation()` in the controller path.

## 4. Platform timers

- [ ] 4.1 Windows (`viewer_win32.cpp`): after a successful document load with `animationActive()`, `SetTimer(hwnd, kGifTimerId, startAnimationDelayMs(), nullptr)`; handle `WM_TIMER` → `controller->animationTick()` → re-arm with the returned delay (or `KillTimer` when 0) → `InvalidateRect(hwnd, nullptr, FALSE)`.
- [ ] 4.2 Windows: stop the GIF timer (`KillTimer`) in the window-destroy path, the document-close path, and the file-swap path, before engine teardown.
- [ ] 4.3 Linux (`viewer.cpp`): a single-shot `QTimer` member restarted with the returned delay after each `animationTick()`, `update()` on the canvas; stop it in document close / file-swap / widget destructor.

## 5. Build and verification

- [ ] 5.1 Build the Windows preset (`cmake --preset windows-x64-release && cmake --build --preset windows-release`); confirm clean compile.
- [ ] 5.2 Build the Linux preset (`cmake --preset linux-release && cmake --build --preset linux-release`); confirm clean compile.
- [ ] 5.3 Smoke test (Windows): open an infinite-loop animated GIF — confirm it animates in place at the declared per-frame delays, the page indicator shows `1 / 1`, and zoom/fit/rotate keep the animation running.
- [ ] 5.4 Smoke test (Windows): open a finite-loop GIF and confirm the animation stops on the last frame after its declared loops; open a single-frame GIF and confirm no timer runs.
- [ ] 5.5 Smoke test (Windows): close the lister and switch to another file mid-animation — confirm no timer leakage (no further repaints, no crash).
- [ ] 5.6 Smoke test (Linux): repeat 5.3–5.5 on the Qt viewer.
- [ ] 5.7 Verify in a debug session that `QImageReader::supportedImageFormats()` includes `gif` in the trimmed static Qt build; if not, confirm the formatted fallback to MuPDF works and file a follow-up to re-enable the gif handler.