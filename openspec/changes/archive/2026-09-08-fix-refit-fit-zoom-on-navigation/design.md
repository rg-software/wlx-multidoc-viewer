## Context

In paged mode the fit zoom is computed by `ViewerController::computeFitZoom()` (viewercontroller.cpp:421), which anchors on the unit containing `m_state.currentPage()` — the unit that was current at open/relayout/zoom-change time. It is only re-run from `openDocument`, `relayout`, `cyclePagePresentation` (single→double transition). Paged `nextPage`/`prevPage` (viewercontroller.cpp:119, 138) merely advance `currentPage` and call `notifyChanged()` — they never recompute the zoom. As a result, on mixed-size documents a non-anchor page whose aspect is taller than the anchor unit's fits to a zoom that overflows the page area, and paged `pageBlockDown`/`pageBlockUp` (viewer_win32.cpp:764, viewer.cpp:449) correctly interpret that overflow as "scroll within the page" — hence two taps per page.

Continuous mode intentionally keeps a single document-wide zoom anchored in the layout (see viewer-scrolling "Zoom and layout changes preserve the viewport anchor"); this design must not break that.

See proposal.md for the user-visible motivation.

## Goals / Non-Goals

**Goals:**
- Paged mode: fit-to-page and fit-to-width recompute against the current view unit on every navigation command, so each unit satisfies the active fit rule independently.
- Paged mode: PageDown/PageUp advance one view unit per press when there is no meaningful vertical overflow (within the normal overlap band).
- Both platforms behave identically (shared controller logic for the fit math; both viewers only need the tolerance tweak).

**Non-Goals:**
- Per-page zoom in continuous mode (kept document-wide and layout-anchored; explicitly excluded by spec).
- Changing fit-to-width semantics for the *whole document* anchor used in continuous mode (`maxRowWidth`, viewercontroller.cpp:470).
- Altering the 32 px `kPageBlockOverlap` band itself.
- Changing manual zoom or rotation behavior.

## Decisions

### D1: Refit fit zoom in paged mode after every navigation command

Add a private helper `refitAfterNavigation()` on `ViewerController`:

- No-op unless `hasDocument() && isPagedMode() && m_fitMode != FitMode::Manual`.
- Capture the current zoom, call `computeFitZoom()`, and **only** call `computeLayout()` + `notifyChanged()` when the resulting zoom actually changed (epsilon compare). Uniform documents become a pure no-op — no relayout, no cache invalidation, no scroll jump.
- When the zoom does change, `computeLayout()` gives the caller a fresh layout; the paged viewers always reset `m_scrollY` to 0 on page change anyway, so no anchor preservation is needed.

Call it in the paged success paths of `nextPage`, `prevPage`, `firstPage`, `lastPage`, `goToPage`. Do **not** call it from the continuous-mode branches (`nextPageInContinuousMode`/`prevPageInContinuousMode`) — those intentionally keep the anchored zoom.

Rationale: refitting at the navigation chokepoint means arrow keys, toolbar next/prev, sidebar clicks, and PgUp/PgDn all get consistent per-unit fit. The epsilon guard makes the cost ~zero on uniform documents and limits relayout churn on mixed ones to pages where the zoom actually moves.

Alternative considered: recompute fit lazily inside `pageBlockDown` only. Rejected — it fixes PgDn but leaves arrow-key/toolbar navigation with wrong fit, and spreads the logic across both viewers instead of one shared chokepoint.

### D2: Paged fit-to-width targets the current unit, not the document-wide widest row

`computeFitZoom`'s FitToWidth branch (viewercontroller.cpp:462) uses `maxRowWidth()` — the doc-wide maximum, correct for continuous mode. In paged mode it should mirror the FitToPage branch and target the *current unit* (same `unitFirst(currentPage)`/`unitBounds` computation). Extract the unit-size computation already present in the FitToPage branch so both fit modes share it.

Rationale: consistent with the "current unit" rule — in paged mode the unit being viewed is what must satisfy the fit rule (see specs/viewer-zoom delta). This changes the `two-page-view` behavior: on a singleton cover in paged fit-to-width, the cover itself now fills the viewport width, and the refit-on-navigation (D1) re-fits the following {2,3} spread when the user navigates to it — so the next spread still fits by width (verified: G51). Continuous mode keeps the doc-wide widest spread as the FitToWidth target (spec unchanged there).

### D3: PageUp/PageDown treat a near-fitting unit as fully visible

In `pageBlockDown`/`pageBlockUp` (viewer_win32.cpp:764, viewer.cpp:449) the guard is `maxY > 0 && m_scrollY < maxY`. Change to a tolerance compare: a unit with `maxY <= kPageBlockOverlap` is "fitting"; only `maxY > kPageBlockOverlap` counts as real overflow. This prevents a 1–31 px overflow (e.g. from `fitToPageZoom` float rounding and `layoutScale`) from costing an extra tap, while a genuinely tall page (manual zoom, or fit-to-width on a portrait page) still scrolls within it.

D1 is the root fix; D3 is the belt-and-suspenders guard so the two-tap case cannot reappear from sub-32-px rounding. Both viewers need the same one-line-guard change.

### D4: Fit mode changes and continuous-mode actions already refit correctly

`cycleFitMode`, `zoomIn/Out`, `setManualZoom`, `rotateCw/Ccw` all route through `relayout(scrollY)` (viewercontroller.cpp:390), which already calls `computeFitZoom` + `computeLayout` and anchors the viewport in continuous mode. In paged mode switching presentation (`cyclePagePresentation`) already refits. No changes needed there; only toggleMode is unaffected by zoom.

## Risks / Trade-offs

- [D1 epsilon guard could mask a real zoom drift] → epsilon 1e-4f is far below the minimum zoom step (1.25×/0.8×); a no-op compare at that threshold cannot freeze a visible change.
- [Per-navigation refit could relayout + invalidate cache on every page of a mixed doc] → mitigated by the epsilon guard: only pages whose fit zoom actually differs relayout. Even then the invalidation is the same work an explicit zoom/fit change already performs.
- [FitToWidth paged now zooms tight to narrow units; a portrait page at that zoom is taller than the viewport and legitimately needs two taps] → this is correct behavior (genuine overflow reveals real content), and spec'd as such in the viewer-navigation delta scenario "PageDown on a unit taller than the viewport scrolls first".
- [Refit could fight a user's manual pan within an overflow page] → refit only runs on *navigation* (unit change), not on scroll, so panning within an overflowing unit is untouched.

## Migration Plan

Behavior fix; no data or config migration. Rollback = revert the change; the open-time anchor behaves as before.

## Open Questions

None.