# Design

## Context

Startup view state is seeded in two places today: `ViewerController`'s constructor applies `viewer_settings::kStartPagedMode`, and `openDocument()` applies `startupFitMode()` from `viewer_settings::kStartFitMode`; page presentation resets to `Single` because `closeDocument()` rebuilds `ViewerState`. All three are read-only INI seeds (`src/viewer_settings.h`). Runtime changes flow exclusively through `ViewerController` commands (`toggleMode`, `cyclePagePresentation`, `cycleFitMode`, and the manual-zoom paths that force `FitMode::Manual`), which is the single choke point shared by toolbar buttons and keyboard shortcuts on both platforms.

Persistence infrastructure already exists: `FavoritesStore` (`src/favorites.*`) is a UI-thread singleton holding `multidocviewer-favorites.json`, resolved once (plugin dir → config dir → temp, write-probed), written atomically via `QSaveFile`, debounced 300 ms (Win32 `WM_TIMER` on a message-only window / Qt `QTimer`), written only on change, with `flush()`, `resetForTesting()` and `setLocationOverrideForTesting()` hooks. Its `loadFrom` reads only `version == 1` and the `favorites` key, so unknown top-level keys are ignored by existing readers. See proposal.md for motivation and `specs/view-mode-persistence/spec.md` for the contract.

## Goals / Non-Goals

**Goals:**
- Opt-in (`[Viewer] AutosaveViewMode`) save/restore of display mode, page presentation, and fit mode — one global record riding in the favorites JSON.
- Zero behavior change when the flag is off (byte-identical store writes, same startup seeds).
- Backward/forward compatible store file: old builds must still read favorites written by the new build and vice versa.

**Non-Goals:**
- Per-document view-state records; persisting page/zoom/rotation/scroll/sidebar state; writing the INI; a settings UI; changing the existing INI seed semantics when the flag is off.

## Decisions

**D1 — Extend `FavoritesStore` rather than add a second store.** The issue explicitly allows storing "along with the user bookmarks", and the store already provides directory resolution, atomic writes, debouncing, write-on-change, and test hooks. A sibling `multidocviewer-viewstate.json` was rejected: it duplicates `resolveDir`/debounce/test plumbing and doubles the failure surface; `QSettings` was rejected: it would introduce a second config mechanism next to the read-only INI rule and lose the write-probed location logic. API: a small `ViewState` struct (`paged`, `presentation`, `fit` as enum-ish values), `viewState()` (returns whether a valid record exists) and `setViewState()` (marks dirty + `scheduleSave()`).

**D2 — Keep `kFormatVersion = 1`; add an optional top-level `viewState` object.** Bumping to 2 would make older builds reject the whole file (`loadFrom` returns early on version mismatch) and silently drop the user's favorites — unacceptable for an opt-in nicety. Additive keys are ignored by old readers and absent keys are handled by new readers, so both directions degrade cleanly. Values are stored as short strings (`"paged"|"continuous"`, `"single"|"double"|"double-cover"`, `"manual"|"page"|"width"`); unknown/missing → no record (fall back to INI seeds).

**D3 — Record at the controller command sites, not in the toolbar presenter or viewers.** Keyboard shortcuts bypass `ToolbarPresenter`, and the two platform viewers differ, so the controller is the only complete choke point. One private `recordViewState()` (guarded by `viewer_settings::kAutosaveViewMode`) is called from `toggleMode()`, `cyclePagePresentation()`, `cycleFitMode()`, and from `zoomIn()`/`zoomOut()`/`resetZoom()` only when the fit mode actually transitions to `Manual`. It is *not* called from `openDocument()`/`closeDocument()`, so restoring never rewrites the store.

**D4 — Restore points mirror the existing seed points.** Constructor: paged mode from the record, else `kStartPagedMode`. `openDocument()`: fit mode from the record, else `startupFitMode()`; presentation from the record, else the `Single` default that `closeDocument()` leaves behind. Runtime toggles still win for the lifetime of the window/document; the record only affects future loads. Precedence: valid record → INI seed → built-in default (per bit, so a partially malformed record still restores the valid bits).

**D5 — Flush on window close.** `ListCloseWindow` (shared in `plugin.cpp`) additionally calls `FavoritesStore::get().flush()`, closing the 300 ms debounce race when the host closes the lister and immediately exits. This also hardens the existing favorites path. Writes inside the store stay debounced for bursty cycling.

**D6 — No new INI write path.** The flag is parsed read-only in `viewer_settings.h` next to `kStartPagedMode` with the existing `parseBool` (`1/true/yes/on`), defaulting to false — same pattern as `SidebarVisible`.

## Risks / Trade-offs

- [Host killed before the 300 ms debounce fires] → D5 flush at `ListCloseWindow` covers the normal close path; a hard kill can lose at most the last burst, which the spec accepts ("provided the write completed").
- [Old build reads a file written by the new build (or vice versa)] → D2 keeps version 1 and additive keys; harness asserts favorites round-trip with and without the `viewState` key.
- [Two lister windows disagree (last writer wins)] → accepted for a global record; documented as scope, not a bug.
- [Store path unwritable → nothing persists] → reuse of the store's write-probe fallback keeps behavior identical to favorites; failures are non-fatal (no `qWarning` spam beyond one line).
- [Restore loop: reading a record triggers writing it] → D3 keeps record writes out of the open/close path, and `saveNow` skips byte-identical serializations anyway.

## Migration Plan

Additive and opt-in: no data migration. Rollback = ship the previous build; the `viewState` key it does not understand is ignored and favorites keep working. The shipped `assets/multidocviewer.ini` documents the new key commented-out (disabled by default).

## Platform-specific code

- **Shared (both platforms):** `viewer_settings.h` flag parse; `FavoritesStore` `ViewState` struct + JSON serialize/parse + `viewState`/`setViewState`; `ViewerController` seeding (constructor, `openDocument`) and `recordViewState()` call sites; `ListCloseWindow` flush in `plugin.cpp`.
- **Windows:** no new Win32 code — the debounce reuses the existing message-only-window `WM_TIMER` in `favorites.cpp`; smoke test under Total Commander (`cmake --preset windows-x64-release`).
- **Linux:** no new Qt code — the debounce reuses the existing `QTimer`; smoke test under Double Commander (`cmake --preset linux-release`). Interactive verification pending in the existing Qt-in-host gap (AGENTS.md).

## Open Questions

None — flag name (`AutosaveViewMode`), scope (mode buttons only), and record scope (global) are settled in proposal.md.
