## Context

See `proposal.md` — Why/What. Current state relevant to the design:

- `ViewerController::openDocument(path)` (viewercontroller.cpp:49) already performs everything the spec's "file switch" requirement needs: it reopens the engine, resets the page, clears render caches, recomputes layout and fit, and fires `notifyChanged()`. Re-opening a sibling is therefore a second `openDocument` call, not new plumbing.
- Paged-mode navigation (`nextPage()`/`prevPage()`, viewercontroller.cpp:95-113) clamps via `ViewerState` and returns `false` at boundaries — the exact point where sibling spanning hooks in.
- Toolbar enablement lives in a single place: `ToolbarPresenter::refreshState()` (toolbar.cpp:16-17).
- The natural-order comparator the spec requires already exists: `naturalCompare()` in comicengine.cpp:27-60 (anonymous namespace).
- Images (JPEG/PNG/TIFF/GIF/BMP/WEBP) are routed to `MuPdfEngine` via the formatdispatcher fallback; the `plugin.cpp` detect string is the canonical raster-format list.

## Goals / Non-Goals

**Goals:**
- A single shared, platform-agnostic folder-context component that discovers, orders, and indexes sibling images.
- Sibling spanning at document boundaries for both navigation paths (paged counter and continuous view) and for both toolbar and keyboard sources.
- Identical behavior on Windows and Linux.

**Non-Goals:**
- Browsing non-image formats (PDF, DjVu, CHM, comic archives) as folder items.
- Panning/ordering by folder creation time or host file list; ordering is filename-based only.
- Persisting the folder position across lister reopenings.
- Animating multi-frame GIFs (separate `animated-gif-playback` change); GIF frames remain pages here.

## Decisions

**D1 — Sibling scanning lives in a new `ImageFolder` helper owned by `ViewerController`; `plugin.cpp` does not change.**
`ViewerController::openDocument()` (viewercontroller.cpp:49) refreshes folder context on every open, so both `ListLoad` and `ListLoadNext` (the WLX in-place swap) get sibling context automatically with one call site. `ImageFolder` uses Qt `QDir::entryList` — Qt Core is linked on both platforms — so the component is a single platform-agnostic file.
*Alternatives rejected:* scanning in `plugin.cpp` (adds a second call site and couples discovery to the host boundary); platform `FindFirstFile`/`opendir` (duplicates logic per platform for no gain).

**D2 — Folder-browsable classification is by file suffix against the raster-image list, not by page count or engine type.**
The set is the detect-string raster formats: `.jpg`, `.jpeg`, `.png`, `.gif`, `.tif`, `.tiff`, `.bmp`, `.webp`. `.cbz` is intentionally excluded even though MuPDF also opens it, because it is a comic archive, not a folder image.
*Alternatives rejected:* "pageCount()==1" (a multi-frame GIF is image-type even though count>1, and TIFF can be multi-page); "engine is MuPdfEngine" (too coarse — that engine also handles PDF/XPS/EPUB/CBZ).

**D3 — Extract `comicengine.cpp`'s `naturalCompare()` into a shared header `src/naturalsort.h`** and use it for both comic entries and folder listing. The comparator stays byte-identical; `comicengine.cpp` swaps to the shared copy.
*Alternatives rejected:* duplicating the 40-line comparator (drift risk — the spec requires both lists to order identically).

**D4 — Boundary spanning hooks into the existing clamp returns.**
- Paged mode: in `ViewerController::nextPage()`, if `m_state.nextPage()` returns `false` and `m_folder->hasNextSibling()`, open the sibling via `openDocument(path, 1)` and return `true`. `prevPage()` mirrors with `openDocument(path, lastPage)`.
- Continuous mode: the toolbar's continuous path (toolbar.cpp:110-113) already handles the view move; when `base == pageCount` and a next sibling exists it calls a controller `openSibling(kNext)`; the controller opens the sibling and the toolbar scrolls to the new top.
- `firstPage()`/`lastPage()`/`goToPage()` remain clamped — the spec only spans *advance* commands (`viewer-navigation` delta is scoped to next/prev).
*Alternatives rejected:* overloading `goToPage`/`first`/`last` to span (violates "jump to document bounds" semantics); making the engine multi-file aware (touches the engine contract unnecessarily).

**D5 — Toolbar enablement reads two new context calls.**
`refreshState()` (toolbar.cpp:16-17) becomes:
- `PrevPage`: `hasDoc && (page > 1 || m_folder->hasPrevSibling())`
- `NextPage`: `hasDoc && (page < count || m_folder->hasNextSibling())`
Both backends (`toolbar_win32.*`, `toolbar_qt.*`) change nothing — enablement is presenter-driven.

**D6 — File switching through boundary navigation reuses `openDocument(path, landingPage)`.**
The existing reset behavior — cache clear, layout recompute, `FitToPage` reapplied, `notifyChanged()` → page indicator refresh — satisfies the spec's "file switch behaves like opening a new document" requirement. The only addition is an optional landing-page parameter so "previous" lands on the sibling's last page.

## Platform-Specific Code

- **Shared (both platforms):** `ImageFolder` (Qt `QDir`), `naturalsort.h`, controller boundary logic, presenter enablement. No `#ifdef` — behavior is identical.
- **Windows:** no new code beyond the shared path; `toolbar_win32.cpp` receives enablement through the existing presenter.
- **Linux:** no new code beyond the shared path; `viewer.cpp` gets the same enablement through `toolbar_qt.cpp`.

## Risks / Trade-offs

- [Directory scan runs on the UI thread on every document open] → Bounded to a single `QDir::entryList` of one folder (typically hundreds of entries); negligible next to the document open that precedes it. Same relative cost as the existing natural sort in `ComicEngine`.
- [`FitToPage` is forced on each sibling switch] → Consistent with today's `openDocument` behavior (same reset on `ListLoadNext`). Preserving the user's fit mode across folder switches would require a fit-mode carry parameter — recorded as out of scope.
- [Multi-frame GIF/TIFF: folder spans only at the outermost boundaries] → Correct per the `image-folder-browsing` spec (spans after the last rendered page); animation is a separate change and will not conflict — it changes frame rendering, not the file-level sequence.
- [Same directory sorted case-insensitively can tie on case-variant names] → `naturalCompare` already lower-cases for comparison and ties fall back naturally to stable `QDir` order; acceptable and deterministic for the lister use case.

## Migration Plan

No data migration. Deploy in one commit; the feature is additive (new component + narrow hooks). Rollback is reverting the commit — no persisted state is touched. The extraction of `naturalCompare` into `naturalsort.h` is behavior-preserving by construction (identical comparator).

## Open Questions

None that would change the specs, approach, or task breakdown.