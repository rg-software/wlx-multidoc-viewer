## Why

The current viewer has a Win32 + Qt split where both paths ad-hoc handle the same basic viewing concerns: render pipeline, navigation, zoom, modes, resize, and DPI. Behavior diverges between platforms and the existing `win32-viewer-improvements` and `add-multidoc-viewer` deltas carry overlapping requirements with some stale assumptions. We need a single, authoritative baseline spec for the core viewing surface that both viewers must satisfy, so we can validate each platform against one contract and close the gaps. This pass also incorporates refined UX behavior (top info panel, rotation, fit-mode cycling, viewport-preserving page jump in continuous mode) that the existing code only partially implements.

## What Changes

- Authoritative baseline spec for the core viewer (render, navigate, zoom, modes, info panel) covering both Windows (Win32) and Linux (Qt6) implementations.
- Top-of-viewport info panel that surfaces current/total page, continuous mode status, and fit-to-page status.
- Rotation by 90° steps clockwise and counter-clockwise, applied as a render transform and persisted per document.
- Fit-mode cycling: a single `F` shortcut cycles fit-to-page → fit-to-width → 100% → fit-to-page.
- Continuous-mode next/prev page jumps one page without repositioning the viewport.
- Continuous-mode mouse wheel scrolls smoothly with page boundaries visible; non-continuous mouse wheel jumps strictly page-to-page.
- Cross-check existing behavior against the new spec; existing changes can be archived once the baseline is implemented.

## Capabilities

### New Capabilities
- `viewer-info-panel`: Top-of-viewport overlay showing current/total page, continuous mode status, fit-to-page status, updated live as state changes.
- `viewer-rendering`: Page-to-bitmap render pipeline, DPI awareness, aspect-ratio preservation, resize behavior, page rotation (90° steps CW/CCW) on both platforms.
- `viewer-navigation`: Page navigation (next/prev/first/last/goToPage), bounds clamping, viewport-preserving next/prev in continuous mode, page jump prompt.
- `viewer-zoom`: Fit-mode cycle (fit-to-page → fit-to-width → 100%), manual zoom step with bounds, auto-fit activation.
- `viewer-display-modes`: Paged vs continuous display mode, keyboard toggle, mouse-wheel behavior in each mode (smooth scroll vs. page jump).

### Modified Capabilities
<!-- None. Existing capabilities are not under openspec/specs/ yet; only delta specs
     exist inside in-progress changes. The two existing changes (add-multidoc-viewer,
     win32-viewer-improvements) will be archived after this baseline lands. -->

## Impact

- `src/viewer_win32.*`, `src/viewer.*` — both viewers must converge on the baseline behavior including rotation, fit cycle, info panel, and continuous-mode jump semantics.
- `src/viewerstate.h` / new `src/viewercontroller.*` — rotation angle and fit-mode cycle need to live on the shared state/controller.
- `src/document.h`, `src/mupdfengine.*`, `src/djvuengine.*` — engines must honor a rotation transform parameter in `renderPage`.
- `src/plugin.cpp` — page indicator now lives in the info panel; host-facing lister copy (`ListSendCommand(lc_copy, ...)`) still receives "current/total" but the in-viewport overlay also includes mode and fit status.
- **Out of scope for this change**: CHM (Microsoft Compiled HTML Help) is not supported by MuPDF and no `chmlib` dependency exists. CHM support will be tracked in a separate change.
