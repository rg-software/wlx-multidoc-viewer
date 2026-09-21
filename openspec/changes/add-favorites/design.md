## Context

See `proposal.md` — Why/What. The reference model is SumatraPDF's `Favorite` (name, pageNo, pageLabel, scrollPos, isTemporary) stored as SquareTree text inside `SumatraPDF-settings.txt` next to the exe. Implementation facts that shape the approach here:

- The plugin has **no write infrastructure today**: `multidocviewer.ini` is read-only (`PluginConfig::get()` parses it once), and the WLX API surface (`wlxplugin.h`) exposes no "open arbitrary file" mechanism (`ListLoad`/`ListCloseWindow`/`ListLoadNext`/`ListSendCommand` only), which bounds favorites to the currently open document.
- Qt6 Core is already statically linked on both platforms, so `QJsonDocument`, `QSaveFile`, `QTimer`, and `QStandardPaths` are available with zero new dependencies.
- The sidebar is already a shared `SidebarBackend` contract (flat pre-order `SidebarEntry`, `addEntry(id, parentId, title)`, lazy children), populated today by flattening the engine outline only; availability is gated on `engine()->outline()` !== empty. Both backends (WC_TREEVIEW / QTreeWidget) expose a collapsible level-0 row, so a flat section below the outline is a list-build change, not a new widget.
- Toolbar state already flows controller → `ToolbarPresenter` → backend and back, and hotkeys live in `viewer_win32.cpp` (`Ctrl+B` is free; `F` is avoided as a TC/DC host key) while the Qt viewer has none yet.
- The saved scope is page-level granularity only (user decision): no scroll offsets, because Sumatra's `scrollPos` only works because it also persists per-file zoom, which this plugin does not.

## Goals / Non-Goals

**Goals:**
- Mark/unmark the current page as a favorite from a toolbar button (checked when active) or `Ctrl+B`, with the outline heading captured as the label when present; no duplicates; toggling again removes.
- Persist per-document favorites across sessions in one shared JSON file, keyed by absolute path, written atomically and only when something changed; `multidocviewer.ini` stays untouched.
- Resolve the store location at most once per process: writable plugin-module directory, else the OS user-config directory; a failed write never loses in-memory data or crashes.
- Show a flat Favorites section (ascending page order, collapsible) directly below the outline in the existing sidebar; activating a favorite jumps to its page (paged: display; continuous: scroll into view); highlight the favorite matching the exact current page.
- Sidebar availability becomes "outline OR favorites"; identical behavior on the Win32 and Qt viewers.

**Non-Goals:**
- Cross-file favorite jumps — the WLX API cannot open another document from inside the lister.
- Scroll-offset/position persistence within a page (Sumatra's `scrollPos`), and any per-file zoom persistence.
- Editing/renaming favorites (no rename UI; toggle only) or a favorite dropdown page-jump menu.
- A split or two-panel sidebar — the favorites section reuses the existing tree below the outline.
- Separating favorites into per-file store files, or keying by content hash (unstable across re-saves).
- Re-anchoring favorites after a reflow changes page mapping (see Risks).

## Decisions

### D1 — One shared JSON store (`multidocviewer-favorites.json`), accessed through a process-wide `FavoritesStore` singleton

`FavoritesStore::get()` mirrors `PluginConfig::get()`'s lazy-frozen singleton: first call resolves the location (below) and loads the file; later calls are cached. API is document-path-scoped: `favoritesFor(path)`, `isFavorite(path, page)`, `toggle(path, page, label)`, plus a change notification the viewers subscribe to. Persistence uses `QJsonDocument` (already linked via Qt6 Core).

*Alternatives rejected:* storing in `multidocviewer.ini` — the INI is deliberately read-only, and mixing mutable favorites into a defaults file muddies "defaults vs. state"; mINI can write but would fork the storage format in two directions; `QSettings` on Windows hijacks the registry and on Linux a host-chosen location, both bypassing the deliberate location policy.

### D2 — Location resolved once per process: writable module directory, else `QStandardPaths::AppConfigLocation`

Writability is probed by actually creating the file on first save (or a test write when the file is absent), not by probing attributes — probing is unreliable for ACLs, network drives, and program-directory virtualization. The resolved directory is frozen for the process lifetime, matching the spec ("resolved at most once per process"). The module directory comes from the same `GetModuleHandleW`/`dladdr` machinery `pluginconfig.cpp` already uses. If the resolved location later becomes unwritable, the write fails gracefully and retries on the next change (spec: "Write failure does not crash").

### D3 — Data model keyed by normalized absolute path

```json
{
  "version": 1,
  "favorites": [
    { "path": "/abs/path/doc.pdf",
      "items": [ { "page": 3, "label": "Chapter 1 Background" }, { "page": 12 } ] }
  ]
}
```

`page` is 1-based public page number; `label` is optional, captured from the outline heading under the reading position at mark time, and never reconciled later. The document key is the canonicalized absolute path with directories on Windows lower-cased (drives/files stay case-insensitive like the Win32 filesystem); the same canonicalizer keys the current document's lookups, so `c:\Docs\A.PDF` and `C:\docs\a.pdf` dedupe to one entry. Unknown fields are ignored on load; a corrupt `version` or JSON drops to an empty in-memory store rather than crashing. Forward-compatible when the schema grows (e.g. a future `scrollPos`).

### D4 — Atomic, debounced writes that skip no-ops

Writes go through `QSaveFile` (write temp + atomic rename), so an interrupted save never corrupts the previous file. A short debounce (~300 ms) collapses toggles during a burst; a flush is forced on lister close (`ListCloseWindow`) and document switch. After each mutation the store compares the serialized state against the last-saved state and skips the write when equal (spec: "No-op does not write"). On write failure the in-memory state is retained and the next change retries.

### D5 — Sidebar merge: favorites rendered as a synthetic level-0 section below the outline

`SidebarPresenter::reload()` builds one flat pre-order list: the engine-outline entries exactly as today, then a synthetic level-0 "Favorites" entry (`resolved = false`, i.e. never navigates, collapsible — the same treatment TOC container rows already get), then one row per favorite, ascending by page, using `addEntry(id, parentId, title)` to keep both tree backends uniform. The section only materializes when the current document has at least one favorite; the sidebar itself becomes available when the outline is non-empty OR the document has favorites (the modified "Outline sidebar availability" requirement).

Highlight policy splits cleanly: favorite rows match the current page **exactly**; the outline's deepest-entry-at-or-before rule is evaluated over outline entries only and never descends into the favorites subtree, so a non-favorite page clears the favorites highlight (spec).

A favorites toggle re-runs `reload()`, whose full `clearEntries()` + rebuild would otherwise reset every expanded branch. To keep the tree's collapse state across reloads, `SidebarBackend` gains two optional hooks (default no-ops): `expandedEntryIds()` (captured before the clear) and `restoreExpandedEntries(ids)` (re-applied after the entries are added). The presenter only restores the capture while the open document is unchanged, since flat ids are document-relative. A user-collapsed Favorites header is absent from the captured set and so stays collapsed; an expanded one is re-expanded.

*Alternatives rejected:* a second tree/panel — duplicates the tree chrome, fights the existing width model, and complicates the personal line-behind; injecting favorites into the outline's own hierarchy — they are not outline nodes and must be excluded from the outline highlight policy anyway; accepting the full-rebuild reset — it hides newly added rows behind a collapsed section and collapses unrelated expanded outline branches on every `Ctrl+B`.

### D6 — Toolbar toggle button + `Ctrl+B` on both platforms

A bookmark control (Material `bookmark` U+E866, existing programmatic-glyph path in `toolbar_icons.cpp`) is added to the shared `Control` enum and placed next to the zoom controls in both `defs[]` (Win32) and `addButton` (Qt) tables. Its checked state is driven by the same controller→presenter→backend state flow as the other buttons: `ToolbarPresenter` re-syncs checkedness on every page change so keyboard, mouse, and toolbar never diverge.

`Ctrl+B` is wired in the Win32 `onKeyDown` switch (free; `F` stays reserved for the host) and, on Qt, becomes the viewer's first shortcut (`QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B))`) — there is no prior art, so this establishes the pattern. Both funnel through `ViewerController::toggleCurrentPageFavorite()`, identical to the button.

## Risks / Trade-offs

- [`Ctrl+B` host collision in TC/DC lister] → Unbound by default in both hosts' lister; verify in the Win32 smoke task and document the fallback (toolbar only) if a user host binds it.
- [Reflowable (EPUB/MOBI) pages shift between sessions] → Page numbers can drift and a reflow can shrink the page count; the spec's "out-of-range no-op" covers the degenerate case and the stored label keeps the row identifiable. True re-anchoring is explicitly a non-goal.
- [Two TC instances write concurrently] → Atomic rename prevents corruption, but last-writer-wins across processes. Accepted: matching Sumatra, which has the same single-settings-file race.
- [Probe-based writability can be wrong under virtualization] → The probe is an actual create/write attempt, so a "writable-looking but failing" directory simply produces the graceful-failure path instead of a crash.
- [Favorites file bloat over many documents] → JSON is tiny (page + label per entry); no index or per-file split is warranted yet.
- [Qt checkbox/look differs from Win32 owner-draw] → Semantic parity (checked emphasis) rather than pixel parity, consistent with the existing cross-platform toolbar note.

## Migration Plan

- New file only; existing `multidocviewer.ini` behavior is untouched. No migration step; a reader of the previous-version file (v0 without `label`) just treats missing fields as "absent". Rollback is a revert plus deleting `multidocviewer-favorites.json`.
- Ship order: shared `FavoritesStore`/model + presenter/toolbar state hooks → Win32 (toolbar row, hotkey, sidebar build) → Qt (toolbar button, QShortcut, sidebar build) → build-gate both presets with the harness style already used (`tests/harness_refit.cpp`-like) before marking tasks complete.
- Smoke to check: toggle → file appears at the resolved location; reopen restores; two files keep separate favorites; collapsed section survives resize; Ctrl+B parity with the button; out-of-range favorite is a no-op.

## Platform-Specific Code

- **Shared (`src/favorites.cpp`/`.h`):** `FavoritesStore` (singleton, resolve-once location, QJsonDocument load/save, QSaveFile atomic writes, debounce timer, change notification); `DocumentEngine` outline-heading lookup helper used at mark time; canonicalized-path keying; `ViewerController::toggleCurrentPageFavorite()` + `isCurrentPageFavorite()` and the sidebar presenters' merge/highlight/collapse-state logic; toolbar `Control` addition + ToolbarPresenter checked-state sync.
- **Windows (`src/viewer_win32.cpp`, `src/toolbar_win32.cpp`, `src/sidebar_win32.cpp`, `src/pluginconfig.cpp`):** `Ctrl+B` in the hotkey switch; bookmark button in the `defs[]` row (checked `BS_PUSH`/owner-drawn state) near the zoom buttons; sidebar `clearEntries`/build appends the flat Favorites section via `addEntry` and captures/restores `TVIS_EXPANDED` state; module-dir resolution reuses the existing `GetModuleHandleW` code.
- **Linux (`src/viewer.cpp`, `src/toolbar_qt.cpp`, `src/sidebar_qt.cpp`):** `QShortcut` for `Ctrl+B` (first viewer shortcut — establishes the pattern); bookmark `QToolButton` with `setCheckable` beside the zoom buttons; sidebar build via `addEntry` + collapsible section, with the shared presenter's capture/restore hooks implementing the section's collapsed state across reloads.

## Open Questions

None — scope, storage, and UI are settled with the user; tasks are scheduled for generation after this design is accepted.