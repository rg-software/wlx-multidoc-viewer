## Context

See proposal.md for the motivation. Current state that shapes the approach:

- `ViewerController::renderVisiblePages()` composes one tall `QImage` strip (height capped at `kMaxStripHeight = 1,500,000 px`), uses a **uniform stride** based on `currentPage()` dimensions, and re-renders + re-copies it on the UI thread whenever `needsStripRerender()` fires.
- The Win32 viewer (`viewer_win32.cpp`) derives its scrollbar range and `maxScrollY()` from that capped bitmap height, so the thumb can never reach the real middle of a long document; the Qt viewer derives its range from the strip's `QPixmap`.
- `recomputeFitZoom()` computes fit zoom against `currentPage()` dims, so the zoom (and thus the whole layout) shifts as the current page changes on mixed-size documents.
- Rendering is synchronous: MuPDF/DjVu `renderPage` is called directly on the UI thread in a loop over visible+buffer pages.

**SumatraPDF reference** (read-only, not copied): `DisplayModel` keeps a virtual `canvasSize` (sum of per-page heights + gaps, no cap), a per-page `PageInfo.pos` rect, and `pageOnScreen = pos - viewPort`. Scrollbar uses `nMax = canvasSize.dy - 1`, `nPage = viewPort.dy`; the thumb therefore reaches `canvasSize.dy - viewPort.dy` — the full reachable range. Paint iterates visible pages and blits each from a **per-page render cache** (grey placeholder while a page renders). `RelayoutKeepingView()` records `dyInPage` for the first visible page, relays out, then restores `viewPort.y = firstVisible.pos.y + dyInPage`.

## Goals / Non-Goals

**Goals:**
- Continuous mode uses a virtual per-page canvas: exact page rects, no uniform stride, scrollbar range = real document height.
- Per-page render cache so scroll cost is proportional to the number of newly-visible pages, not the document.
- Fit-zoom relayouts preserve the viewport anchor on both platforms.
- Win32 paints per-page; Qt keeps its `QLabel` strip but sources it from the shared layout API.

**Non-Goals:**
- No async render threads (engines are thread-hostile; tracked in a separate `async-render-worker` change).
- No tiling, no predictive rendering, no thumbnails.
- No change to `DocumentEngine` or the engines.

## Decisions

### Decision 1: Controller owns a per-page pixel layout

`ViewerController` builds `QVector<QRect> m_pageRects` (device px, after zoom + rotation + DPI) plus derived `m_contentSize` (width = max page width centered; height = sum of page heights + gaps). Rebuilt whenever zoom / rotation / viewport size / DPI changes.

- *Effect:* all scroll math (`pageAtScrollOffset`, `maxScrollOffset`, scrollbar range, hit-testing) uses real geometry; `pageStride()` and the uniform-stride assumptions in `viewercontroller.cpp` are removed. `pageAtScrollOffset` becomes a binary search over page tops.

### Decision 2: Scrollbar range == content height, off no bitmap

Win32 scrollbar is set to `nMin=0`, `nMax = contentHeight - 1`, `nPage = viewportHeight`, `nPos = scrollY` (exactly Sumatra's numbers, which make the full range reachable). `maxScrollY()` returns `contentHeight - viewportHeight`. The Qt `QScrollArea` value is likewise set from `contentHeight`.

- **Effect:** thumb drag position is proportional to document position; the `kMaxStripHeight` cap disappears from the scroll path (its memory-bound job moves to Decision 3).

### Decision 3: Per-page render cache with viewport clamp

Controller exposes `QImage renderPageCached(page)` returning the in-cache bitmap for a page (rendering synchronously on cache miss) and guarantees: pages not within `[firstVisible - buffer, lastVisible + buffer]` are evicted after composition, and never rendered speculatively. A small constant cache window (`viewer_settings::kCacheWindowPages`, ~8) bounds memory independently of document length.

- **Effect:** scrolling revisits cached pages without re-rendering; a fresh page costs only

### Decision 4: Anchored zoom/layout recompute in the controller

`recomputeFitZoom()` gains an anchor: the caller passes the current `scrollY`; the controller computes `firstVisiblePage = pageAtScrollOffset(scrollY)`, `dyInPage = scrollY - pageRect(page).top()`, applies zoom/rotation/layout, then returns `pageRect(page).top() + dyInPage` as the new scroll position for the viewport to restore (Sumatra's `RelayoutKeepingView`).

- **Effect:** fit-zoom no longer shifts the document; mirror the existing capture/restore scroll code but centralize the math.

### Decision 5: Win32 paints per-page; Qt renders the visible window

- Win32 keeps a single composite viewport image as today but builds it **only from the visible window** via `renderCachedViewport(scrollY)` (a bounded slice ~viewport + cache buffer), and blits pages individually from per-page `HBITMAP`s. Rendering new pages marks the viewport dirty; paint copies the composed viewport.
- Qt: `renderCachedViewport(scrollY)` replaces `renderVisiblePages()` strip compose (still a `QPixmap` for the `QLabel`), but its height is the viewport window, not the full doc; the scroll area's range comes from Decision 2 math. Qt paint migration to a custom widget is deferred (see Non-Goals).

### Decision 6: scroll feed stays in the viewers

Wheel/keyboard/drag handlers keep scrolling the view's own scroll value; they just consult `maxScrollOffset()` and `pageAtScrollOffset()` from the controller (already largely true). Only `onVScroll` gets the corrected range+position semantics of Decision 2.

## Alternatives considered

- **Keep the strip, raise cap only** — rejected: does not fix the uniform-stride misalignment, does not speed up the per-scroll re-composition, and pins a fragile cap.
- **Async render + tiles (full Sumatra port)** — see Non-Goals; engines are single-context and not thread-safe, so a real worker pool needs engine-level cloning (tracked as `async-render-worker`).
- **Compute per-page layout lazily on scroll** — rejected for simplicity: full layout build must remain O(pages) anyway for the range; building the vector of rects is negligible vs render cost.

## Platform-specific code

| Concern | Windows (`ViewerWin32`) | Linux (`ViewerWidget`) |
|---|---|---|
| Continuous source | Per-page `QImage`s → per-page `HBITMAP`s; viewport slice comp | `QLabel`+`QPixmap` from `renderCachedViewport` |
| Scrollbar range | `SCROLLINFO` `nMin=0 nMax=contentHeight-1 nPage=vh nPos=scrollY` | `QScrollBar::setRange(0, contentHeight-scrollAreaHeight)` |
| `maxScrollOffset` | single path from controller | same |
| Scroll input | `WM_VSCROLL` / `WM_MOUSEWHEEL` / drag | `QScrollBar::valueChanged` / wheel / drag |
| Fit anchor apply | controller returns new scrollY; `ScrollWindow`/paint applies it | apply via `QScrollBar::setValue` after layout change |

Everything else (layout build, cache, range math, anchor math) lives in the shared controller.

## Risks / Trade-offs

- [Risk: synchronous render of a newly-visible page still stalls one frame] → Mitigation: bounded to ~1-2 whole pages per scroll step (the demo it replaces re-rendered the entire document strip); follow-up `async-render-worker` removes the residual stall.
- [Risk: eager layout build (O(pages)) on open/zoom] → Mitigation: per-page gauge read is cheap (`pageDimensions`); build is a single pass; acceptable at open and on user-initiated zoom/rotation only.
- [Risk: Qt painter divergence while it stays on `QLabel`] → Mitigation: both viewers call the same `renderCachedViewport()`; only the surface differs; the widget rewrite is a follow-up tracked in tasks as out-of-scope.
- [Risk: cache window tuning] → Mitigation: expose `viewer_settings::kCacheWindowPages` constant; measure with the 200-page PDF during spec walkthrough.

## Open Questions

- Qt scroll-ratio mapping: `QLabel` insists the widget be the full document strip to keep scroll proportional through `QScrollArea`. For Qt we may need `setWidgetResizable(false)` + a synthetic tall widget OR accept the strip-as-window and implement thumb dragging via `QScrollBar` event overrides. Resolve the smallest viable change during task 4.x; if the QLabel constraint dominates, Qt lands a custom paint widget instead (deferred).