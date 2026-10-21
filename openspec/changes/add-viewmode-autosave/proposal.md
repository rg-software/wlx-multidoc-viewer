# Proposal

## Why

The viewer's mode buttons (Paged/Continuous, single/double presentation, Fit cycle) reset on every relaunch: `[Viewer] PagedMode` / `[Viewer] FitMode` are static INI values the user must hand-edit, and the presentation state is not configurable at all (issue #22). Users who routinely flip the same switches want the plugin to remember them automatically.

## What Changes

- New read-only INI flag `[Viewer] AutosaveViewMode` (default `false`). When absent or false, behavior is exactly as today.
- When enabled, the current Paged/Continuous mode, page presentation (single / double / double-with-cover), and fit mode (manual / page / width) are saved to the plugin's shared user JSON store — the same atomic, debounced file the bookmarks/favorites live in — and restored on the next load, taking precedence over the static `PagedMode` / `FitMode` INI seeds.
- Scope is one global last-used record (not per-document), applied on both the Win32 viewer (Windows) and the Qt viewer (Linux). Rotation, manual zoom, sidebar state, and current page are out of scope.
- The INI file itself stays read-only.

## Capabilities

### New Capabilities

- `view-mode-persistence`: Autosaving and restoring the last-used view-mode state (paged/continuous, page presentation, fit mode) behind the INI flag — what is recorded, when it is written, where it is stored, and how it seeds the next load on both platforms.

### Modified Capabilities

- `plugin-config`: Add the `[Viewer] AutosaveViewMode` flag key requirement (boolean, read-only, absent/malformed = disabled).
- `viewer-display-modes`: Amend "Default mode is continuous" — the startup mode/presentation now seed from the autosaved record when the flag is enabled and a record exists, otherwise from today's defaults/INI values.

## Impact

- `src/viewer_settings.h` — parse the new INI flag alongside `kStartPagedMode` / `kStartFitMode`.
- `src/favorites.*` (or a sibling store) — serialize/parse the extra `viewState` record in the existing JSON, keeping version compatibility and the atomic debounced write.
- `src/viewercontroller.*` — seed paged mode / presentation / fit mode from the restored record at construction and document open; record changes when the mode commands run.
- `src/plugin.cpp`, `assets/multidocviewer.ini` — flag documentation / wiring if a load- or close-time flush is needed.
- Tests: existing settings/favorites harnesses plus a new/extended harness covering save → simulated restart → restore, and flag-off no-op.
