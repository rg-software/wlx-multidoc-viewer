## Why

In paged mode, fit-to-page/fit-to-width zoom is computed once against the unit that was current when the document opened (or last re-laid-out) and frozen across navigation. On mixed-size documents (e.g. scanned handbooks with wider spreads among text pages), every page whose aspect is taller than that anchor unit overflows the viewport by a few pixels to a full line. PgDn then scrolls *within* the page on the first tap and only advances on the second (page 1, 2, 2, 3, 3, 4…). The user expectation in paged mode is that fit modes fit the page currently being viewed.

## What Changes

- Paged mode: after every navigation command, recompute the active fit zoom against the current view unit instead of keeping the open-time anchor. In fit-to-page each paged unit fits the page area exactly; in fit-to-width it fits the current unit's width (not the document-wide maximum row).
- Continuous mode: unchanged. A strip cannot have per-page zooms, so it keeps one document-wide zoom anchored on the unit in view; the "no fit recompute against the selected page" rule stays.
- PageUp/PageDown: a unit that fits (or would fit within the normal block-overlap) advances to the next unit on the first press; only a unit with genuine overflow scrolls within it.
- Both platforms (shared controller + Win32/Qt viewers).

## Capabilities

### New Capabilities
- none

### Modified Capabilities
- `viewer-zoom`: fit-to-page and fit-to-width in paged mode track the current view unit during navigation, not an open-time anchor.
- `viewer-navigation`: PageDown/PageUp advance one view unit per press in paged mode when the current unit fits the viewport under the active fit mode.
- `viewer-display-modes/two-page-view`: in paged mode fit-to-width targets the current unit (a singleton cover fills the width; the following spread is re-fitted on navigation); in continuous mode it keeps targeting the document-wide widest spread.

## Impact

- `src/viewercontroller.cpp` / `.h`: navigation paths (`nextPage`, `prevPage`, `firstPage`, `lastPage`, `goToPage`) recompute fit zoom in paged mode; `computeFitZoom` gains a paged FitToWidth branch that targets the current unit.
- `src/viewer_win32.cpp` / `src/viewer.cpp`: `pageBlockDown`/`pageBlockUp` accept a fit-tolerance so a page that fits (within the overlap band) advances immediately.
- `tests/` harnesses updated for the single-tap advance.