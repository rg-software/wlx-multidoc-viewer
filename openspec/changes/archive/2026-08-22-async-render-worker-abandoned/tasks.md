## 1. Shared worker + cache seams (depends on continuous-scroll-performance)

- [x] 1.1 Add a platform-agnostic `RenderWorker` class (single worker thread, FIFO queue of `PageRenderRequest{page, zoom, dpiScale, rotation, epoch}`, storage of results into the shared cache, `pageRendered(page)` callback)
- [x] 1.2 Add mutable cache epoch: cache keying (page, zoom, rotation, dpiScale); any layout change increments epoch, drops stale entries, and discards in-flight requests of the prior epoch
- [x] 1.3 Add Drop-on-dequeue: worker discards a request whose page is outside the current visible window + buffer, or whose params/epoch are stale
- [x] 1.4 Add placeholder state: controller reports "visible but not ready" for pages missing from the cache; expose that to both viewers
- [x] 1.5 Add teardown protocol: `close()`/document swap increments epoch, drains the queue, and waits for the worker ack before dropping the document

## 2. Engine thread-safety

- [x] 2.1 MuPDF: create a worker-dedicated `fz_context` via `fz_clone_context(m_ctx)` used only for `renderPage`; keep metadata (page count, cached dimensions) readable on the UI path with a mutex; drop the clone at `close()`
- [x] 2.2 DjVu: serialize all `renderPage` on the worker via a mutex (single context), ensure metadata reads either take the mutex or read immutable page geometry
- [x] 2.3 Add a thread-safety contract note to `DocumentEngine` (`renderPage` is worker-only; metadata is UI-safe via mutex)

## 3. Viewers

- [x] 3.1 Win32: `ViewerWin32` subscribes to `pageRendered`; handler posts/processes `WM_APP+1` and invalidates only the affected page rect; paint renders placeholder for in-flight pages
- [x] 3.2 Qt: `ViewerWidget` connects `pageRendered` via `Qt::QueuedConnection`, `QTimer` (or queued signal), and `update()`s the page rect; placeholder painted for missing pages
- [x] 3.3 Remove the synchronous `renderPageCached` path from the scroll loop; all renders now go through the worker (cache hits stay on the UI read path)

## 4. Spec walkthrough

- [x] 4.1 (harness-scroll C1: placeholder on miss, worker fills + repaints; rendering moved fully off UI thread) Walk `specs/viewer-async-render/spec.md` on Win32: fast drag stays responsive, placeholders appear and fill in, only the affected region repaints
- [x] 4.2 (harness-scroll C2: zoom bumps epoch, in-flight old renders dropped, re-render at new params) Walk render-parameter invalidation: zoom/rotation during renders yields no stale bitmap, no flicker
- [x] 4.3 (harness-scroll C3: close + reopen during render in flight drains cleanly) Verify teardown: close (or open another doc) while renders are in flight does not crash and drains cleanly
- [x] 4.4 Verify build: `cmd /c "vcvarsall x64 && cmake --preset windows-x64-release && cmake --build --preset windows-release"`

## 5. Follow-ups

- [ ] 5.1 If profiling warrants, propose a worker pool (clone context per worker) as a separate change