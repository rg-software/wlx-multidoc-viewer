## Context

See `proposal.md` - Why. Relevant current state (all in `src/viewercontroller.cpp`):

- `openDocument` -> `computeFitZoom` -> `computeLayout`, and `computeLayout` (line ~608) loops over every page calling `m_engine->pageDimensions(page)`.
- `computeLayout` also clears `m_pageCache`, `m_linkCache`, and bumps `m_layoutEpoch` on every call - it is the "everything changed" path, not a geometry-only path.
- `m_pageRects`/`m_contentSize` are pure geometry derived from a `QVector<QSize> sizes`; the render cache stores `QImage`s keyed by page, independent of position.
- `relayout(scrollY)` (line ~478) already captures "first visible page + fractional offset" and restores it after recomputation.
- Worker -> UI marshaling already exists: `ViewerController::setUiMarshal` (`viewercontroller.h:242`), installed by `viewer_win32.cpp:160` (PostMessage) and `viewer.cpp:310` (Qt queued). `SearchController` is the precedent for a cancel/join worker.

DjVuLibre constraint (from the abandoned `async-render-worker` change): one `ddjvu_context_t`/`ddjvu_document_t` is not safely accessed concurrently; all ddjvu calls must be serialized, and only one thread may pump the context at a time.

## Goals / Non-Goals

**Goals:**
- Open cost independent of page count for DjVu: measure only the current view unit before first paint.
- Converge to exact per-page geometry for the whole document after open, without blocking or janking the UI.
- Refinement must be visually inert: no viewport jump, no re-render of unchanged pages.
- Page count and direct jump stay exact and immediate.
- Confine the change to DjVu so MuPDF/comic/image behavior is byte-for-byte unchanged.

**Non-Goals:**
- Replacing DjVuLibre with `kjk/djvudec`. Only reconsider if the spike shows *decode*, not *measurement*, dominates (see Risks).
- Progressive/partial page rendering, tiled rendering, or reworking the render cache policy.
- Lazy sizing for reflowable engines (MuPDF EPUB/MOBI/FB2), whose layout genuinely changes as content is measured.
- Changing paged-mode geometry (already measures only the current unit via `refitAfterNavigation`).

## Decisions

### D1: Laziness is opt-in per engine

Add `virtual bool prefersLazyPageSizing() const { return false; }` to `DocumentEngine`; only `DjVuEngine` overrides it to `true`.

- *Why:* MuPDF/comic/image engines measure all pages cheaply (and MuPDF reflowable layout must know all pages), so keeping their eager path removes any regression risk from provisional geometry. The blast radius of the change becomes DjVu-only.
- *Alternative:* make lazy layout universal - rejected (reflowable engines change layout as pages are measured, and the estimate/refine machinery would be pointless overhead there).

### D2: Open measures only the current view unit

When `prefersLazyPageSizing()` is true, `openDocument` measures the current unit (1-2 pages) and uses the first measured page's `PageInfo` as the provisional size for every other page. `computeFitZoom` and the initial layout use measured sizes for the unit and provisional sizes elsewhere. Engine calls at open: O(1).

- *Why:* the first render must be exact, and the current unit is the only page the user sees before refinement. Uniform scanned DjVu (the common case) makes the provisional size exact for the whole document, so no refinement ever fires.
- *Alternative:* sample N pages spread through the document for a better estimate - rejected for now: adds open cost and still needs refinement; revisit only if mixed-size docs prove common.

### D3: Split layout computation from invalidation

Refactor `computeLayout` into:
- `buildLayoutFromSizes(const QVector<QSize>&)` - pure geometry: rebuild `m_pageRects` + `m_contentSize`, O(N) arithmetic, **no cache clearing, no epoch bump**.
- `invalidateLayout()` - the current behavior (clear `m_pageCache`/`m_linkCache`, bump `m_layoutEpoch`), used only when zoom/rotation/DPI/fit actually changes.

A refinement updates `sizes[page]`, calls `buildLayoutFromSizes`, re-anchors the scroll, and repaints.

- *Why:* this is the glitch-safety mechanism. Cached `QImage`s and link hot zones are position-independent, so a geometry-only rebuild can keep them. Only zoom/rotation/DPI changes invalidate them.
- *Alternative:* keep calling `computeLayout` on refinement - rejected: it would discard every cached render and re-render the viewport on each refined page.

### D4: Measurement scheduler - UI prefetch plus one completion worker

- **UI-thread prefetch:** before composing a frame, ensure pages in `[firstVisible - buffer, lastVisible + buffer]` are measured. On-screen geometry is therefore always exact; the user never sees an estimated page under the cursor for long.
- **Completion worker:** a single `std::thread` walks the remaining unmeasured pages and delivers results through `setUiMarshal` (same pattern as `SearchController`), applying refinements on the UI thread. Cancel/join on `closeDocument`/document swap, mirroring `stopSearchThread`.
- **Serialization:** `DjVuEngine` guards every ddjvu call with a `std::mutex`, so the worker and the UI never pump the context concurrently. The UI holds the lock across a render (decode included); the worker waits, then resumes. The only UI-side wait is for a single in-flight measure.
- *Why:* the UI prefetch guarantees correctness where it is visible; the worker guarantees the scrollbar/fit converge for regions the user never visits.
- *Alternative:* UI-thread idle-batched measurement only (no thread) - simpler and lock-free, but measuring a large document in UI-time chunks either takes very long or janks; keep as the fallback if the spike shows total measurement is small (see Risks).

### D5: Refinement consequences are bounded

- Refining page widths can change the continuous fit-to-width target (`maxRowWidth`). When that happens, re-fit once with the same epsilon guard as `refitAfterNavigation` (line ~251) and invalidate the render cache only if the zoom actually changed.
- Fit-to-page targets the current unit only, so refining distant pages never changes it.
- The scroll anchor is captured and restored exactly as `relayout(scrollY)` already does.

### D6: Page count is untouched

`m_pageCount` already comes from `ddjvu_document_get_pagenum` (directory) at open (`djvuengine.cpp:65`), independent of `pageDimensions`. No code change; the `viewer-navigation` scenario is a regression guard, not new behavior to implement.

## Risks / Trade-offs

- [Refinement shifts content on mixed-size documents] -> anchor via first-visible page + fraction (existing `relayout` logic); only pages after the refined one move; no cache invalidation (D3).
- [Worker races DjVuLibre's single-context model] -> one mutex in `DjVuEngine`; single thread pumping at a time; cancel/join before teardown (D4), exactly like `SearchController`.
- [A worker measure stalls the UI] -> if the spike shows per-page measurement above ~10 ms, make the UI prefetch use a try-lock and fall back to provisional geometry for that frame (worker uses the blocking call). Threshold comes from the spike.
- [Continuous fit-to-width re-fits once after load on mixed-size docs] -> epsilon-guarded; document it as expected, and note uniform docs never re-fit.
- [Provisional scrollbar range misleads] -> range is always complete (every page reachable) and converges; `viewer-scrolling` scenarios cover this.
- [Regression in non-DjVu formats] -> impossible by construction: the flag is false and the eager path is unchanged (D1).
- [Decode, not measurement, dominates] -> the spike reports both. If decode dominates, reopen the `djvudec` question (issue #19) as a separate change; this change is still correct and useful on its own.

## Migration Plan

No data or API migration. The behavior change is per-format and opt-in. Rollback is reverting the change or flipping `DjVuEngine::prefersLazyPageSizing()` back to false, which restores today's eager layout.

## Platform-specific code

| Concern | Windows (`ViewerWin32`) | Linux (`ViewerWidget`) |
|---|---|---|
| Refinement notification | `setUiMarshal` -> `PostMessage` (already installed at `viewer_win32.cpp:160`) | `setUiMarshal` -> Qt queued invoke (already installed at `viewer.cpp:310`) |
| Worker thread | shared `std::thread` in the controller, no platform code | same |
| Engine serialization | `std::mutex` inside `DjVuEngine`, no platform code | same |

All new logic is shared; the only platform touch points are the existing `UiMarshalFn` installers, which already exist for search. Windows x64/x86 and Linux are affected identically.

## Open Questions

- Batch size and yield cadence for the completion worker (how many pages per marshaled update). Purely a tuning detail; does not affect the specs or the approach.
