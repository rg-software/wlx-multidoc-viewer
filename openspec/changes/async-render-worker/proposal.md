## Why

After the `continuous-scroll-performance` change, continuous mode renders only pages near the viewport from a per-page cache, but page rendering still happens synchronously on the UI thread. A newly-revealed page (e.g. after a fast scrollbar jump or a large wheel step) stalls the UI for one or more frames. SumatraPDF's reference behavior is to render off the UI thread and paint a placeholder until each bitmap is ready; this change moves rendering off the UI thread on both platforms.

## What Changes

- Add a background render worker owned by the engines: MuPDF renders through a dedicated worker-owned `fz_context` (per confer-up on task), DjVuLibre likewise serialized on the worker; `DocumentEngine` gains thread-safety boundaries rather than being called concurrently by accident.
- `ViewerController`'s per-page cache becomes a shared cache: the UI thread reads it, the worker fills it. A page not yet in the cache is rendered as a neutral placeholder (with "rendering page N" indicator) until the worker completes and invalidates the region.
- Render requests are bounded to the visible window + cache buffer; requests for pages that scroll out of view are cancelled/dropped so a fast drag never queues the entire document.
- Both viewers hook a `pageRendered(page)` signal (Win32: custom window message; Qt: queued signal) to repaint only the affected page.

## Capabilities

### New Capabilities
- `viewer-async-render`: background page rendering, placeholder display for in-flight pages, and invalidation-on-complete for both viewers.

### Modified Capabilities
<!-- None: viewer-scrolling from continuous-scroll-performance gains async semantics at archive time; no existing main spec to delta against. -->

## Impact

- `src/document.h` — thread-safety contract for `renderPage` and engine lifetime.
- `src/mupdfengine.*` — per-worker `fz_context` (clone or dedicated), shared `fz_document`.
- `src/djvuengine.*` — serialize via worker + mutex; DjVu contexts are not thread-safe, so all renders run on the single worker.
- `src/viewercontroller.*` — shared page cache, render request queue, `pageRendered` callback, placeholder state.
- `src/viewer_win32.*`, `src/viewer.*` — wire up `pageRendered` → region invalidation; paint placeholder for missing pages.
- Depends on `continuous-scroll-performance` (per-page layout + cache API) being implemented first.