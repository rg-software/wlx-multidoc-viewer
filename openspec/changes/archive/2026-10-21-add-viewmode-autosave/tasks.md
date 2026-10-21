# Tasks

## 1. INI flag and shared user-store record

- [x] 1.1 In `src/viewer_settings.h`, add `kAutosaveViewMode` parsed with the existing `parseBool` from `[Viewer] AutosaveViewMode` (default `false`), documented next to `kStartPagedMode`/`kStartFitMode` as a read-only seed; verify by adding a `harness-startup` scenario whose per-process INI sets `AutosaveViewMode=true` (and one with an absent/malformed value) and asserting the parsed value via the harness's existing ini-spawn mechanism.
- [x] 1.2 In `src/favorites.h`/`.cpp`, add the `ViewState` record (paged flag, presentation, fit — stored as short strings) with `bool hasViewState()`/`ViewState viewState()`/`void setViewState(const ViewState&)`, serialized as an optional top-level `"viewState"` object while `kFormatVersion` stays `1`; verify with a new `harness-favorites` case that sets a record, `flush()`es, `resetForTesting()`s, and reads the same values back.
- [x] 1.3 Add store compatibility cases to `tests/harness_favorites.cpp`: a legacy file with only `favorites` loads favorites and reports no view state; a file with favorites plus `viewState` loads both; a malformed/absent `viewState` is ignored without dropping favorites; verify by running `harness-favorites` and seeing all cases pass.
- [x] 1.4 Document the new key in `assets/multidocviewer.ini` (commented out, disabled by default) describing that enabling it saves/restores paged/continuous, presentation, and fit mode next to the bookmarks and takes precedence over `PagedMode`/`FitMode`; verify the shipped ini still parses (existing `harness-startup` default scenario) and the comment matches the implemented semantics.

## 2. Controller seed and record

- [x] 2.1 In `src/viewercontroller.cpp`, seed paged mode in the constructor from the stored record when `kAutosaveViewMode` is on and a record exists, otherwise from `kStartPagedMode`; verify with a `harness-startup` scenario that pins `FavoritesStore::setLocationOverrideForTesting` to a store containing `paged`, sets no conflicting INI, and asserts `isPagedMode()`.
- [x] 2.2 In `openDocument()`, seed fit mode and page presentation from the record (per bit) ahead of `startupFitMode()`/`Single`, keeping `closeDocument()`'s existing paged-mode preservation; verify with `harness-startup` scenarios: record `fit=width` beats `[Viewer] FitMode=page`, record `presentation=double-cover` is reflected by `pagePresentation()`, and a record with an unknown fit value falls back to the INI seed.
- [x] 2.3 Add a private `recordViewState()` guarded by `kAutosaveViewMode` and call it from `toggleMode()`, `cyclePagePresentation()`, `cycleFitMode()`, and `zoomIn()`/`zoomOut()`/`resetZoom()` when the fit mode transitions to `Manual` — never from `openDocument()`/`closeDocument()`; verify with a `harness-startup` scenario that each command updates the store's view state and that a zoom before/after toggle produces the expected fit value.
- [x] 2.4 Verify flag-off and no-rewrite behavior in `harness-startup`: with `AutosaveViewMode` absent no store write happens (store byte content unchanged after toggling), and with the flag on, opening/restoring a document does not rewrite the store (serialized JSON identical before/after open).

## 3. Persist on window close

- [x] 3.1 In `src/plugin.cpp`, call `FavoritesStore::get().flush()` from the shared `ListCloseWindow` teardown (both `#ifdef` branches reach it) so the 300 ms debounce cannot drop the last change; verify by building the Windows binary and confirming a mode change followed by closing the lister survives an immediate host exit in the task 4.3 smoke test.

## 4. Windows verification

- [x] 4.1 Build and run the harnesses: `cmake --preset windows-x64-release && cmake --build --preset windows-release`, then run `harness-startup` and `harness-favorites`; verify every new case passes.
- [ ] 4.2 Interactive smoke test in Total Commander: with `AutosaveViewMode=true`, cycle paged/continuous, presentation (all three), and fit (all three), close the lister, reopen the plugin, and confirm the toolbar reflects the restored state; confirm the record beats conflicting `PagedMode`/`FitMode` INI values; set the flag to `false` and confirm startup follows the INI seeds again and the store's `viewState` key is left untouched.
- [ ] 4.3 Confirm the close-time flush: toggle a mode and close the lister immediately, then exit Total Commander at once and relaunch — the last toggle is still restored.

## 5. Linux verification

- [ ] 5.1 Build and run the harnesses: `cmake --preset linux-release && cmake --build --preset linux-release`, then run `harness-startup` and `harness-favorites`; verify every new case passes.
- [ ] 5.2 Interactive smoke test in Double Commander (or Total Commander under Wine if a native host is unavailable): repeat the 4.2 restore/precedence/flag-off checks and confirm the same restored toolbar states as Windows.
