## Context

The viewers already hit-test pages and transform canvas points into page space for text selection (`wordAtCanvas`/`pageTransform` in `ViewerController`, `pageUnderPoint`/`clientToCanvas` on Win32, `pageAtCanvas`/`widgetToCanvas` on Qt) and already jump to destinations for the outline (`goToPage`). Neither engine extracts hyperlinks: `MuPdfEngine` never calls `fz_load_links`, and `ChmEngine` opens each topic as an isolated one-page HTML document (`src/chmengine.cpp:478`) so relative hrefs have nowhere to resolve. See proposal.md — Why/What for motivation and scope, and specs/viewer-hyperlinks for the behavior contract.

Platform constraint: Windows is a pure-Win32 viewer (no Qt event loop); Linux is Qt6 Widgets. All shared logic must stay platform-agnostic, with only cursor and URL-launch differing per platform.

## Goals / Non-Goals

**Goals:**
- A single engine-level link model (hot-zone rect + internal destination or external URI) with a no-op default, so unlinked formats are unaffected.
- Extract and resolve links in the MuPDF engine for all MuPDF-rendered documents (PDF/EPUB/XPS/MOBI/FB2), including the synthetic MOBI cover offset.
- Make CHM topic links live by resolving relative hrefs against the archive's page list, including `#fragment` anchors.
- One shared hit-test + follow path in the controller; per-platform cursor and OS launcher.

**Non-Goals:**
- No link annotations rendering/decoration changes (link text already renders; we do not add boxes/highlights).
- No link support for the DjVu or standalone-image engines (DjVu has no text layer; images carry no links).
- No editing/creating links, no link target preview, no plain-click activation.
- No back/forward link history.

## Decisions

### D1. `pageLinks()` is a new virtual with an empty default, alongside the existing capability virtuals

Add a `LinkItem { QRectF bbox; int destPage; float anchorY; QString uri; }` struct and `virtual QVector<LinkItem> pageLinks(int page) { return {}; }` to `DocumentEngine` (`src/document.h`). This mirrors the existing `hasSelectableText`/`supportsSearch` no-op-default pattern, so `DjVuEngine`, `ComicEngine`, and `ImageEngine` are correct without changes and the controller never casts to a concrete engine.

- Alternatives: a link service keyed off the dispatcher (rejected — couples routing to one feature); dynamic-cast to `MuPdfEngine` (rejected — same coupling the text-selection design avoided).

### D2. Extract links with `fz_load_links` and resolve with `fz_resolve_link_dest`

In `MuPdfEngine::pageLinks`, load the body page, call `fz_load_links`, and for each link call `fz_resolve_link_dest`. Chosen over `fz_resolve_link` because `fz_link_dest` carries both the `fz_location` (chapter + page, required for reflowable EPUB/MOBI) and the destination `x`/`y` coordinates used for anchors. `fz_page_number_from_location` flattens chapters into the engine's public page index; `+1` converts to 1-based, `+1` more when `m_bodyHasCover`. `fz_is_external_link` classifies the URI; external links keep the raw URI and leave `destPage = 0`. All calls wrapped in `fz_try`/`fz_catch` per engine convention, under `m_mutex`.

- Alternatives: parsing the PDF `/Annots` ourselves (rejected — reimplements MuPDF); `fz_resolve_link` (rejected — loses coordinates and chapter info needed for anchors/EPUB).

### D3. Link rectangles use the same page-space convention as `TextWord.bbox`

`LinkItem.bbox` is y-down, unrotated, pre-zoom page space, exactly like `TextWord.bbox` (`src/document.h:34`). Hit-testing transforms the pointer through the existing per-page `pageTransform` (zoom + rotation + layout), so links work unchanged in paged, continuous, rotated, and HiDPI states. `anchorY` is normalized `0..1` of the target page height.

- Alternatives: storing canvas/device rects per layout (rejected — must be recomputed on every zoom/rotate/resize and duplicates the transform that already exists).

### D4. CHM links resolve relative hrefs against the archive page list

`ChmEngine` already opens each topic as a one-page MuPDF HTML document; those links surface from the same `fz_load_links` call with the raw href as `uri`. Resolution: reject clear external schemes via `fz_is_external_link`; otherwise split off any `#fragment`, percent-decode, resolve the remainder relative to the current topic's archive path (handling `./`, `../`, leading `/`, and backslashes), then map it through the existing `normalizePath`/`pageIndexOf` to a page. A same-topic `#fragment` (or a resolved cross-topic target) is resolved to `anchorY` by opening that topic's HTML document and calling `fz_resolve_link_dest` on the bare `#fragment`; when it does not resolve, the page top is used.

- Alternatives: rendering each topic and scraping `<a href>` from raw HTML with a parser (rejected — MuPDF already exposes the links from the same pipeline that renders the page, so geometry and anchors come for free).

### D5. The controller owns hit-testing, follow, and anchor application; the platform injects an external launcher

`ViewerController` gains `int linkAt(int page, const QPointF& canvasPt)` (returns an index into the page's links, using `pageTransform` like `wordAtCanvas`) and `bool followLink(int page, int linkIndex, int scrollY)`. Internal links route through `goToPage` (reusing re-fit and scroll anchoring) and then apply `anchorY`; external links invoke an injected callback. The platform installs the launcher once via `setExternalLinkHandler(fn)` — mirroring the existing `setUiMarshal` pattern — so shared code contains no `ShellExecute`/`QDesktopServices`.

- Alternatives: doing hit-test + navigation in each viewer (rejected — duplicated logic, and it would split link behavior from the shared controller that owns page geometry); a Qt-only `QDesktopServices` call in shared code (rejected — shared code must not depend on Qt Widgets on Windows).

### D6. Ctrl+click gating and the hover cursor live in the platform viewers

The viewers already own modifier state and the `WM_SETCURSOR`/`MouseMove` cursor paths. On Win32, `WM_SETCURSOR` returns `IDC_HAND` when `linkAt` hits, and `WM_LBUTTONDOWN` checks `GetKeyState(VK_CONTROL) < 0` before following instead of starting a selection. On Qt, the hover branch sets `Qt::PointingHandCursor` when `linkAt` hits, and `MouseButtonPress` checks `Qt::ControlModifier` before following. Link cursor takes precedence over the text I-beam; plain press keeps today's selection/drag semantics.

- Alternatives: plain-click activation with a drag threshold (rejected — would perturb text selection and miss the chosen gesture); holding Ctrl changing the cursor (rejected — a stable hand cursor is the familiar PDF-viewer affordance).

### D7. Anchor application is a clamped in-page scroll after navigation

For an internal link, navigate to the destination page first (paged mode re-fits as usual), then, when `anchorY > 0`, target `pageTop + round(anchorY * pageHeight)` and clamp to the page's scrollable range. Pages that fit the viewport ignore the offset (the anchor is effectively at the top), and continuous mode uses the existing `scrollOffsetForPage`-based anchor. A destination page that does not resolve (`destPage` outside `[1, pageCount]`) is a no-op.

- Alternatives: adding a page+offset destination type to `goToPage` (rejected — widens the public navigation contract for a link-only concern; a follow-up clamped scroll keeps `goToPage` unchanged).

### D8. Links are cached per page in the controller, invalidated on open/close

`linkAt` runs on every mouse move, so the controller caches `QHash<int, QVector<LinkItem>>` populated on first access for a page and cleared when the document opens or closes (and on engine re-layout). Extraction is cheap and per-page, matching the existing `m_textCache` approach.

- Alternatives: extracting all pages up front (rejected — needless work on large documents); calling the engine on every mouse-move (rejected — MuPDF page load per event).

## Risks / Trade-offs

- **R1: Link hot zones overlap selectable text** → Resolved by the Ctrl+click gesture and cursor precedence; plain press keeps selection. Covered by the modified `viewer-interaction/text-selection` scenarios.
- **R2: Stale link rects after reflowable relayout** → `MuPdfEngine` lays reflowable documents out once at open and viewer zoom does not re-run layout, so rects stay valid for the session; the cache is cleared on open/close and any re-layout. If a future relayout path is added, it must invalidate the link cache.
- **R3: CHM relative-path edge cases** (backslashes, `%20`, case, `mailto:`/`javascript:` hrefs) → Normalize + percent-decode + case-insensitive match via the existing `normalizePath`; `javascript:`/unknown inner anchors resolve to page top or are ignored rather than crashing. Covered by a CHM link harness.
- **R4: `fz_is_external_link` treats any `:` as a scheme** → A pathological relative filename containing `:` would be misrouted to the OS handler; acceptable and consistent with MuPDF's own viewer behavior.
- **R5: Anchor coordinates vary by destination type** (`FIT`, `XYZ`, ...) → Use `dest.y` only for coordinate-bearing types and fall back to page top otherwise, so no wrong scroll is applied.
- **R6: Windows hosts may intercept Ctrl** → Modifier is read from `GetKeyState` at the mouse-down moment; the viewer HWND receives the click regardless of host bindings.
- **R7: Qt `mailto:` handling on Linux** → `QDesktopServices::openUrl` dispatches through xdg-open; a missing handler is a silent no-op as required by the spec.

## Platform-Specific Code

- **Shared (no `#ifdef`):** `LinkItem` + `pageLinks()` (`src/document.h`); link extraction/resolution (`src/mupdfengine.*`, `src/chmengine.*`); link cache, `linkAt`, `followLink`, anchor application, and the external-launcher callback (`src/viewercontroller.*`).
- **Windows (`src/viewer_win32.cpp`):** `IDC_HAND` in `WM_SETCURSOR`; `GetKeyState(VK_CONTROL)` gating in `WM_LBUTTONDOWN`; `ShellExecuteW(nullptr, L"open", uri, ...)` as the external launcher.
- **Linux (`src/viewer.cpp`):** `Qt::PointingHandCursor` in the hover branch; `Qt::ControlModifier` gating in `MouseButtonPress`; `QDesktopServices::openUrl(QUrl(uri))` as the external launcher.

## Migration Plan

Additive, no persisted state or format changes, so there is no migration. Rollback is reverting the change; existing documents are unaffected because `pageLinks()` defaults to empty and all new UI paths are gated on a link hit.

## Open Questions

- Whether to also support plain-click activation behind a future setting (deferred; the chosen gesture is Ctrl+click).
- Whether PDF named destinations should be surfaced as anchors in addition to `XYZ` coordinates (deferred; coordinate-bearing destinations cover the common case).
