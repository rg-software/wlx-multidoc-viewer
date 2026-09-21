## Why

Users want to mark pages they care about in a document and jump back to them across sessions — SumatraPDF-style page favorites. Today nothing persists beyond the current session: no per-document bookmarks, no way to restore a reading mark on reopen.

## What Changes

- Add a per-file favorites store: one shared `favorites.json` (all document paths keyed in it), resolved next to the plugin module when the module dir is writable and in the OS user-config dir otherwise. Loaded once, mutated through a process-wide singleton, saved debounced and atomically (`QSaveFile`). The existing read-only `multidocviewer.ini` is never written.
- A favorite records the current page index plus an optional label captured from the outline heading at that page; a favorite is `{ page, label? }`. Toggling an already-favorited page removes it. Favorites are scoped per document path; while a file is open, only its favorites are shown.
- Toolbar: a bookmark button next to the zoom controls toggles the current page's favorite status and is shown "checked" when the current page is a favorite. Identical behavior on Windows (Win32 toolbar) and Linux (Qt toolbar).
- Hotkey `Ctrl+B` toggles the current page's favorite status on both platforms (the first Keyboard shortcut wired into the Qt viewer).
- Sidebar: the favorites of the currently open document appear in a dedicated collapsible section directly below the outline entries, in the same tree, as a flat non-hierarchical list. Sidebar availability widens from "document has an outline" to "document has an outline or favorites". Favorite rows are highlighted by exact page match; outline rows keep their current deepest-entry-at-or-before position highlight.

## Capabilities

### New Capabilities
- `favorites`: page-level favorites of the open document — toggle status from toolbar and keyboard, jump from the sidebar section, per-file JSON persistence with writable-location detection and atomic writes.

### Modified Capabilities
- `viewer-toolbar`: the outline sidebar availability requirement expands so the sidebar and its toggle are available when the document has favorites even without an outline; the control strip gains the favorites-toggle button.

## Impact

- New files: `src/favoritesstore.*` (store, JSON serialization, location resolution, debounced atomic save).
- Modified: `toolbar.h`/`toolbar.cpp`/`toolbar_win32.cpp`/`toolbar_qt.cpp`/`toolbar_icons.*` (bookmark control), `viewer_win32.cpp` (Ctrl+B) and `viewer.cpp` (first Qt shortcut), `sidebar.h` + both sidebar backends (favorites section, availability gating, highlight policy).
- Qt6 Core only (QJsonDocument/QSaveFile/QStandardPaths) — already linked on both platforms; no new dependencies.
- `pluginconfig.*` and `multidocviewer.ini` remain read-only.