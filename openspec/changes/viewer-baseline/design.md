## Context

Today the viewer has a Win32 implementation (`src/viewer_win32.*`) and a Qt implementation (`src/viewer.cpp` + `src/viewer.h`). Both wrap a `DocumentEngine` returned by `createEngine()` and share a `ViewerState` (in `src/viewerstate.h`) that holds page index, zoom, auto-fit flags, and paged/continuous mode.

Looking at the two implementations side by side reveals the current architecture:

- **Shared**: `DocumentEngine` interface, `MuPdfEngine`, `DjVuEngine`, `ViewerState`, format dispatch in `src/formatdispatcher.cpp`.
- **Duplicated**: fit-zoom math (`applyFitZoom` vs `updateZoomForFit`), DPI detection (`dpiScale()`), per-page render call, page indicator overlay, keyboard shortcut map, mouse-wheel handling.
- **Genuinely divergent**: paint backend (Win32 GDI `BitBlt` vs `QScrollArea`+`QLabel`+`QPixmap`), scroll mechanism (`WS_VSCROLL`/`WS_HSCROLL` vs `QScrollBar`), window parenting (`SetParent`+`SetWindowPos` vs `QWidget` lifecycle).

Known gaps that the spec calls out:

1. **Continuous mode is not actually continuous.** Both viewers render only the current page. `m_state.isPagedMode()` toggles a flag but the render loop ignores it.
2. **No top info panel.** Today the page indicator is painted inline (`DrawTextW` on Win32, `m_counterLabel` on Qt) and shows only current/total + a `Page`/`Cont` tag. The user wants a dedicated info panel that also exposes fit-to-page status.
3. **No rotation.** Neither viewer rotates pages.
4. **Fit-to-width and fit-to-page are independent flags.** The new spec replaces them with a three-state cycle driven by a single `F` shortcut.
5. **Continuous-mode next/prev does not preserve viewport.** Today they re-render and reset scroll.

Engines already accept a `dpiScale` parameter and multiply it into `effectiveZoom = zoom * dpiScale`, so the DPI plumbing exists end-to-end. CHM (Microsoft Compiled HTML Help) is not supported by MuPDF, no `chmlib` dependency exists, and it is out of scope for this change — it will be tracked separately.

## References

**SumatraPDF** (https://github.com/sumatrapdfreader/sumatrapdf) is the reference implementation for many of the decisions in this design. We will consult it (not copy it) for:

- **Continuous-mode scrolling**: how SumatraPDF lays out a vertical page strip, handles wheel events for smooth scroll, and advances pages without re-paging on small wheel ticks. Cited in Decision 2 and Decision 3.
- **CHM support**: SumatraPDF has an existing CHM engine we can study when we plan `add-chm-support` (task 8.1).
- **Window-mode transition**: paged ↔ continuous swap, especially how SumatraPDF anchors scroll position across the transition. Cited in `viewer-display-modes` requirement "Switch from continuous to paged".
- **Engine abstraction**: SumatraPDF's `EngineBase` (C++ base class for PDF/XPS/DjVu/CHM engines) is the gold-standard example of a platform-agnostic engine interface — comparable to our `DocumentEngine`.

We follow our own naming and class layout; SumatraPDF is read-only reference.

## Goals / Non-Goals

**Goals**
- Single source of truth for the basic viewing/browsing surface across Win32 and Qt.
- Eliminate behavioral drift between platforms for navigation, zoom, modes, DPI, and aspect ratio.
- Pull duplicated state logic into shared helpers so each viewer is mostly paint code.
- Implement true continuous mode with smooth mouse-wheel scrolling and page borders visible.
- Implement viewport-preserving next/prev in continuous mode.
- Implement the three-state fit-mode cycle.
- Add rotation (90° CW/CCW) as a per-document render transform.
- Add the top info panel that surfaces current/total, continuous status, and fit-to-page status.

**Non-Goals**
- Outline / bookmarks navigation, text extraction, search, and print are not part of this change.
- No toolbar — host (TC / DC) provides navigation UI.
- No new engines. CHM support is out of scope and will be planned separately.
- No change to format dispatch (which format → which engine stays as in `formatdispatcher.cpp`).

## Decisions

### Decision 1: Add a shared `ViewerController` that owns state + commands

Move page navigation, zoom math, mode toggle, rotation, and the `dpiScale` query into a shared `ViewerController` class. Each viewer keeps its own paint backend but delegates every state transition and render-request to the controller.

- *Effect:* `ViewerState` stays as a pure data holder; the controller exposes `nextPage()`, `prevPage()`, `goToPage()`, `cycleFitMode()`, `zoomIn()`, `zoomOut()`, `setManualZoom()`, `toggleMode()`, `rotateCw()`, `rotateCcw()`, plus a `stateChanged()` signal/callback that the viewers subscribe to in order to repaint and the info panel subscribes to in order to re-render.

### Decision 2: Continuous mode renders a vertical strip in the shared controller

`ViewerController::renderVisiblePages()` returns a single `QImage` covering all pages of the document stacked vertically in continuous mode (current page only in paged mode). The engine is called once per page and the controller composes the strip. Each page is centered horizontally in the strip (left margin == right margin). The strip width is the wider of the rendered page width and the viewport width. The strip height is unbounded (all pages); memory is bounded by the zoom floor (~10%) which keeps pageHeight small for large documents.

- *Reference:* SumatraPDF renders the same way — a tall strip with all visible pages stacked. Its `EnginePage::GetPageMostVisible()` and wheel handler are the model for our `stateChanged()` + strip-recompute loop.
- *Effect:* Win32 paints one big `HBITMAP`; Qt gets a single big `QPixmap` for the scroll area. The page gap between consecutive pages is a fixed small constant (4 device pixels) owned by the controller. With the zoom floor at 10%, a 1000-page A4 document produces a strip ~300k px tall at ~400 px wide (~360 MB) — acceptable for modern systems.

### Decision 3: Continuous-mode page jump preserves viewport

`ViewerController::nextPageInContinuousMode()` / `prevPageInContinuousMode()` advance the current page index by ±1 WITHOUT changing the scroll position. Internally: record the current scroll Y, change page index, recompute the strip, then re-anchor scroll Y so the same viewport slice is shown. This works because in continuous mode the strip is a single tall bitmap and a page is exactly `pageHeight + gap` pixels tall.

- *Reference:* SumatraPDF's continuous-mode PgUp/PgDown advances by one full viewport, but its "next/prev page (one page jump)" preserves the visual anchor — same principle, slightly different scroll delta.
- *Effect:* matches the spec scenario "Next from split view" (1/3 of page N + 2/3 of page N+1 → 1/3 of page N+1 + 2/3 of page N+2) exactly.

### Decision 4: Fit-mode is a three-state cycle on the controller

`ViewerController::cycleFitMode()` transitions `Manual → Fit-to-page → Fit-to-width → Manual (100%)`. Manual remembers the last user-set zoom so `0` (reset to 100%) leaves the controller in manual mode rather than re-entering the cycle. The info panel observes the cycle and reports `Fit to page: ON` only in the fit-to-page state.

- *Alternatives considered:* two independent flags (current behavior). Rejected — leads to ambiguous states (both off, both on) and the user wants a single `F` shortcut.
- *Effect:* the previous `m_state.setFitToWidth()` / `m_state.setFitToPage()` setters are replaced by `cycleFitMode()` plus a `setManualZoom()` for `+/-`/`0`.

### Decision 5: Rotation is a render-transform parameter passed to engines

`DocumentEngine::renderPage(page, zoom, dpiScale, rotation)` gains a rotation parameter (one of `0`, `90`, `180`, `270`). Engines compose the rotation into the matrix they pass to the underlying renderer (MuPDF `fz_matrix` concat, DjVuLibre transform). The controller stores the rotation angle on its state and passes it on every render.

- *Alternatives considered:* post-process rotation in the viewer (rotate the QImage after the engine returns). Rejected — produces blurry text at non-multiples-of-90 and is wasteful for the common 90° case.
- *Effect:* the engine interface gets a new parameter; engines and their callers must update in lockstep.

### Decision 6: Info panel is a shared widget, owned by the controller

A new `InfoPanel` (Win32: child `STATIC` window painted with `DrawText`; Qt: `QFrame` with three `QLabel`s) lives at the top of the viewport. The controller exposes a `stateChanged()` signal; the panel reads current page, total, continuous flag, and fit-to-page flag from the controller's state object and re-renders. The panel height is fixed (e.g. 20 device pixels) and is subtracted from the viewport when computing fit zooms.

- *Effect:* removes the inline `DrawTextW` and `m_counterLabel` from the viewers.

### Decision 7: DPI scale is computed once per render, not stored in state

The viewer queries the host DPI on every render and passes it to `renderPage()`. We deliberately do not cache it on `ViewerState` because DPI can change at runtime (laptop docked/undocked, monitor swap).

### Decision 8: Aspect-ratio preservation is enforced by the controller, not the engine

Engines continue to receive `(zoom, dpiScale, rotation)` and render at the natural page aspect ratio (after rotation). The controller is responsible for computing `zoom` from the current viewport so the page never gets stretched.

### Decision 9: Host page indicator still uses `ListSendCommand(lc_copy, ...)`

The `lc_copy` payload carries only `"<current>/<total>"`. The richer mode + fit state lives in the in-viewport info panel. Hosts that read `lc_copy` get the page count; hosts that don't get the panel.

### Decision 10: Keyboard contract is identical across platforms

Single mapping table owned by the controller:
- Navigation: `Right` / `PageDown` → next, `Left` / `PageUp` → prev, `Home` → first, `End` → last, `G` → go-to-page dialog.
- Zoom: `+` / `=` → in, `-` → out, `0` → 100%.
- Fit: `Shift+V` → cycle fit mode.
- Mode: `V` → toggle paged/continuous.
- Rotation: `R` → rotate CW, `Shift+R` → rotate CCW.

Win32 maps virtual keys; Qt maps `QKeySequence`. The mapping lives in one place to prevent drift.

## Platform-specific code

| Concern | Windows (`ViewerWin32`) | Linux (`ViewerWidget`) |
|---|---|---|
| Window creation | `CreateWindowExW` child of `ParentWin`, `WS_CHILD \| WS_VSCROLL \| WS_HSCROLL` | `QScrollArea` inside `QFrame`; `SetParent`/`SetWindowPos` in `ListLoad` |
| Paint | `BeginPaint` → `BitBlt` from `HBITMAP` (QImage → DIB) | `QScrollArea` paints `QPixmap::fromImage(QImage)` |
| Info panel | Static `STATIC` control above scroll area, painted with `DrawTextW` | `QFrame` at top of vertical layout with three `QLabel`s |
| Scroll | Native `SB_VERT`/`SB_HORZ` with `SCROLLINFO` | `QScrollBar` inside `QScrollArea` (after subtracting panel height) |
| DPI source | `GetDpiForWindow` (Win10+) with fallback to 96 | `QScreen::logicalDotsPerInchX() / 96.0f` |
| Wheel | `WM_MOUSEWHEEL` → page flip in paged, smooth scroll in continuous | `QWheelEvent` → same mapping via controller |
| Keyboard | `WM_KEYDOWN` with virtual key codes | `QShortcut` per binding |
| Host indicator | `ListSendCommand` with `lc_copy` (TC), in-viewport info panel as canonical source | Same `ListSendCommand` path, in-viewport panel as canonical source |

Everything outside this table lives in the shared controller.

## Risks / Trade-offs

- **[Risk] Continuous mode render of large documents is expensive.** *Mitigation:* zoom floor at 10% bounds page dimensions; for very large documents (>2000 pages) the strip may exceed 200 MB. A safety cap of 1.5M pixels prevents runaway memory. If this proves insufficient, a future change can introduce render-on-demand (painting only pages within ±N viewports of the scroll position).
- **[Risk] `ListSendCommand(lc_copy, ...)` behavior differs between TC and Double Commander.** *Mitigation:* the info panel is the canonical source; the copy buffer is best-effort.
- **[Risk] Moving state into a controller changes the public surface of the viewer classes.** *Mitigation:* the WLX entry points in `plugin.cpp` still take `HANDLE` and only call `loadDocument`/`closeDocument`. Internal refactor only.
- **[Risk] Rotation on DjVu is slower than on MuPDF.** *Mitigation:* measure both engines during task 2; if DjVu is significantly slower, consider caching one rotated page in memory.
- **[Risk] Info panel height eating into fit-to-page zoom could surprise users with very small lister windows.** *Mitigation:* clamp the info panel height to a sensible fraction of the viewport (e.g. 20% max); if the panel would dominate the viewport, hide the page indicator portion of it.

## Open Questions

- Should continuous mode render at the same DPI scale as the current page, or downsample to save memory? Will resolve during implementation by measuring on a 200-page PDF and consulting SumatraPDF's continuous-mode behavior on long documents.
- ~~`R` vs `Shift+R` for CW/CCW conflict~~ — **RESOLVED**: keep `R` = CW, `Shift+R` = CCW; no existing binding conflicts.
- ~~Info-panel visibility persistence~~ — **RESOLVED**: always visible while a document is open; not persisted.
- ~~Info-panel height clamping for very small lister windows~~ — **RESOLVED**: clamp the panel height to ≤ 20% of viewport; if the page area would be unusable, hide the page indicator portion of the panel.
