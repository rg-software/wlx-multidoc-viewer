## 1. Shared favorites store and persistence (platform-agnostic)

- [ ] 1.1 Add `src/favorites.h`/`favorites.cpp`: `FavoritesStore` singleton (lazy, `FavoritesStore::get()`, mirroring `PluginConfig::get()`). Public API scoped by document path: `favoritesFor(path)`, `isFavorite(path, page)`, `toggle(path, page, label)`, plus a change-notification hook the viewers/subscribers bind to. Key the store by the canonicalized absolute path (directories lower-cased on `_WIN32`, matching the Win32 case-insensitive FS) so `c:\Docs\A.PDF` and `C:\docs\a.pdf` dedupe.
- [ ] 1.2 Resolve the store location at most once per process: prefer the plugin-module directory (reusing the existing `GetModuleHandleW`/`dladdr` machinery in `pluginconfig.cpp`), probing writability by an actual create/write attempt rather than attribute probing; else `QStandardPaths::AppConfigLocation`. File name: `multidocviewer-favorites.json`.
- [ ] 1.3 Load the store with `QJsonDocument` (Qt6 Core, already linked): tolerate unknown fields, a missing file, and a corrupt JSON/unknown `version` by falling back to an empty in-memory store instead of failing. Map the `{version, favorites:[{path, items:[{page, label?}]}]}` schema from design D3.
- [ ] 1.4 Persist atomically with `QSaveFile` (temp + rename), debounced (~300 ms) to collapse toggle bursts, skipping the write when the serialized state equals the last-saved state; force a flush on document switch and lister close. A failed write retains the in-memory favorites, never crashes, and retries on the next change.
- [ ] 1.5 Add `tests/harness_favorites.cpp` (pattern after `tests/harness_refit.cpp`) and a `harness-favorites` CMake target: assert toggle→save→reload roundtrip, no-op write skips the file (mtime unchanged), corrupt-JSON resilience, canonical-path dedup, location-fallback behavior with an injectable directory, and flush-on-switch.

## 2. Shared controller, sidebar, and toolbar wiring (platform-agnostic)

- [ ] 2.1 Add a shared helper that, given the open `DocumentEngine` and a 1-based page, returns the deepest outline heading whose range covers that page (or empty when none) for use as the favorite's label at mark time.
- [ ] 2.2 In `ViewerController`, add `toggleCurrentPageFavorite()` and `isCurrentPageFavorite()` using `FavoritesStore::get()` for the current document path; on page changes and favorite changes, drive the toolbar checked-state and sidebar rebuild-notify through the existing presenter flow.
- [ ] 2.3 Extend `SidebarPresenter`'s flat pre-order build: outline entries exactly as today, then a synthetic level-0 "Favorites" entry (`resolved=false`, collapsible, no navigation) followed by the document's favorite rows in ascending page order; omit the section entirely when the document has no favorites. Surface a `hasSidebarContent` (outline OR favorites) query for the viewers' availability gate.
- [ ] 2.4 Split the highlight policy: favorite rows match the current page exactly and never participate in the outline's deepest-entry-at-or-before rule; activating a favorite row navigates to its page (paged: display; continuous: scroll into view), guarding against a page beyond the current range (no-op).
- [ ] 2.5 Add a shared `Control::ToggleFavorite` (bookmark) to the toolbar `Control` enum with an activate callback into `toggleCurrentPageFavorite()`; `ToolbarPresenter` mirrors its checked state exactly like the other state-driven buttons so keyboard, mouse, and toolbar never diverge. Add the programmatic bookmark glyph (Material `bookmark` U+E866) to `toolbar_icons.cpp`/`.h`.

## 3. Windows (viewer_win32 / toolbar_win32 / sidebar_win32)

- [ ] 3.1 Add the bookmark toggle to the Win32 toolbar `defs[]` row next to the zoom controls, rendered with the existing owner-drawn checked state; on click invoke `toggleCurrentPageFavorite()`.
- [ ] 3.2 Wire `Ctrl+B` into the `viewer_win32.cpp` hotkey switch (free; `F` stays reserved for the host) to `toggleCurrentPageFavorite()`, honoring the keyboard-focus-neutrality rule for toolbar edit boxes.
- [ ] 3.3 Change the Win32 sidebar availability gate from "outline present" to "outline OR favorites for the open document"; when a toggle turns the favorites set non-empty for a document without an outline, show/reload the sidebar; keep the tree's section-collapse state across reloads.
- [ ] 3.4 Force a `FavoritesStore` flush on `ListCloseWindow` in `plugin.cpp`/`viewer_win32.cpp` and build the Win32 preset (`cmake --preset windows-x64-release && cmake --build --preset windows-release`); verify toggle writes `multidocviewer-favorites.json` at the resolved location, reopen restores, distinct files keep separate favorites, collapse survives resize, and `Ctrl+B` matches the button.

## 4. Linux (viewer.cpp / toolbar_qt / sidebar_qt)

- [ ] 4.1 Add a checkable `QToolButton` with the shared bookmark icon beside the zoom controls in `ToolbarQt` (`setCheckable`, checked state bound to the presenter), activating `toggleCurrentPageFavorite()`.
- [ ] 4.2 Add the viewer's first keyboard shortcut in `viewer.cpp`: `QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B))` → `toggleCurrentPageFavorite()`, establishing the pattern for future Qt hotkeys and respecting toolbar edit-box focus.
- [ ] 4.3 Apply the same availability gate and reload wiring in `ViewerWidget` as 3.3 (sidebar appears when favorites exist without an outline; section collapse persists across reloads), and flush favorites on document switch and window close.
- [ ] 4.4 Build the Qt preset (`cmake --preset linux-release && cmake --build --preset linux-release`); verify the same scenarios as the Win32 build plus the QTreeWidget collapse state and shortcut-driven toggle.

## 5. Cross-platform verification

- [ ] 5.1 Build both presets cleanly with no new warnings; re-run the existing harnesses (harness-refit, harness-icc, harness-favorites) on Windows and confirm no regressions.
- [ ] 5.2 Interactive smoke on both platforms: toggle from toolbar and from `Ctrl+B` matches; favorites file appears at module dir (or user config dir when read-only); reopen restores labels and ordering; favorite jump works in paged and continuous mode; non-favorite page clears the favorites highlight; a sidebar appears for a favorites-only document without an outline and hides when both are absent.
- [ ] 5.3 Re-run `openspec validate add-favorites` and `openspec status --change add-favorites --json`; sync design.md with any decisions that changed during implementation, then mark the change complete per the OpenSpec workflow.