## Context

See proposal.md. This change builds directly on `continuous-scroll-performance`: the per-page layout, `renderPageCached(page)`, cache window, and `pageRendered` boundary are the seam it introduces. Without that seam none of this design applies; alignment is assumed done.

Threading reality of the engines:
- MuPDF (`MuPdfEngine`) owns one `fz_context` (`m_ctx`) used for open/render/text. A `fz_context` is single-threaded by design; a `fz_document` can be shared across cloned contexts (`fz_clone_context`), which is the classic multi-thread render arrangement. MuPDF also supports lock callbacks for sharing one context.
- DjVuLibre (`DjVuEngine`) uses a `ddjvu_context_t`; a context can technically be shared but document access is not safely concurrent. Simplest correct choice: the worker serializes all shell renders behind one mutex.

## Goals / Non-Goals

**Goals:**
- Zero UI-thread blocking on page render.
- One bounded render worker that both engines share.
- Invalidation on completion, placeholder otherwise.
- Stale in-flight renders discarded on parameter change.

**Non-Goals:**
- No per-page tiles, no multi-thread parallel render of one page, no predictive rendering — each page is a single worker task.
- No change to `DocumentEngine`'s signature; only a thread-safety contract note plus any internal context split.

## Decisions

### Decision 1: Single background worker task queue

A single worker thread owns a FIFO of `PageRenderRequest {page, zoom, dpiScale, rotation, epoch}`. The UI thread posts requests; the worker renders and stores into the shared cache, then fires `pageRendered(page)`. A single worker avoids multi-engine lock interleaving and matches DjVu's serialization constraint while still removing all UI-thread blocking.

- *Effect:* bounded parallelism (1), simple ordering, trivially upgradeable to a pool later. FIFO means the newest visible page isn't prioritized; see Decision 3.

### Decision 2: MuPDF render context is worker-owned

`MuPdfEngine` creates a second `fz_context` via `fz_clone_context(m_ctx)` dedicated to render calls. All `renderPage` work happens on the worker through this clone; the UI thread only reads metadata (page count, dimensions cached at layout). The worker context is dropped at `close()`.

- *Alternative considered:* mutex around one shared context. Rejected: MuPDF locking is finer-grained per operation; a clone context is the documented, safer pattern and keeps layout reads unblocked.
- *Open risk:* vector/raster pages that share document resources across clone contexts are safe per MuPDF docs, but verifying dark/transparent tint interplay is part of the walkthrough tasks.

### Decision 3: Requests are dropped/priority-adjusted on scroll

The worker checks each dequeued request against the current visible window + cache buffer; if the page is no longer in that window or its epoch/params are stale, the request is discarded without rendering. This bounds work to what the eye can see and makes a fast drag cheap.

- Re-prioritize: when the viewport window moves, the request queue is purged and re-populated with the new window's pages missing from cache. Newest window → first render.

### Decision 4: Shared cache with mutable epoch

The controller's cache entries become `(QImage, params, epoch)`. Any layout-affecting change increments `epoch` and both drops stale entries and abort in-flight requests of the prior epoch. Placeholder state for visible-but-not-ready pages is a bool on the cache entry (or simply "entry missing").

### Decision 5: Completion notification per viewer

Window: custom `WM_APP+n` posted to the viewer HWND with the page number, handler invalidates only the page's rect. Qt: `QTimer`/queued signal `pageRendered(int)` connected through `QObject`, `update()` on the page.

## Alternatives considered

- Worker pool with per-context clones (Sumatra's `kMaxRenderThreads`) — rejected for this change: engines + GDI paint would need extra locking/tile sync, and DjVu can't parallel, so pool = marginal gain now. A pool is a clean upgrade path from a single FIFO if profiles warrant.
- Render-layer model (QGraphicsView items) — rejected; too big a surface change for both viewers at once.

## Platform-specific code

| Concern | Windows (`ViewerWin32`) | Linux (`ViewerWidget`) |
|---|---|---|
| Worker ownership | controller-owned shared worker | same (shared) |
| Notify done | `PostMessage(m_hwnd, WM_APP+1, page, 0)` | queued emit of a signal connected with `Qt::QueuedConnection` |
| Repaint region | `InvalidateRect` on the page rect | `m_pageLabel->update(pageRect)` |
| Order of worker vs message loop | worker is platform-agnostic class; viewer subscribes | same |

The worker itself is a shared, platform-agnostic class in the controller module.

## Risks / Trade-offs

- [Risk: MuPDF clone-context interplay with in-progress layout reads] → Mitigation: metadata reads remain on UI path with `bound_page` (cheap) or a mutex around document-op / close. Walkthrough verifies on a 200-page doc.
- [Risk: DjVu serialization means slower wall-clock than MuPDF worker render] → Mitigation: one page at a time is still strictly better than forcing it on the UI thread; the placeholder makes latency invisible.
- [Risk: placeholder flicker] → Mitigation: placeholder fill matches canvas bg exactly; only the dirty region invalidated on completion.
- [Risk: race between `close()`/document swap and in-flight renders] → Mitigation: `epoch` increment doubles as the teardown gate: worker drains its queue checking epoch and exits that doc's stage; `close()` waits for the worker to ack an empty queue before `fz_drop_document`.

## Open Questions

- Lock granularity for MuPDF metadata reads — resolve during implementation; likely a simple `QMutex` around metadata + close, no architectural divergence.
- Placeholder "rendering page" text: SumatraPDF draws a text indicator; this design keeps the placeholder purely neutral (bg + optional indicator). If the indicator is wanted on both platforms it is a one-line option in the worker notification.**