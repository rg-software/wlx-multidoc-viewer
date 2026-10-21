# View-Mode Persistence Specification

## Purpose

Remembers the user's last-used view mode (paged/continuous, page presentation, fit mode) in the plugin's shared user store behind an opt-in INI flag, and seeds the next load from it — identically on the Win32 viewer (Windows) and the Qt viewer (Linux).

## Requirements

### Requirement: Autosave gate

The viewer SHALL record and restore view-mode state only when `[Viewer] AutosaveViewMode` in `multidocviewer.ini` is enabled. When the key is absent or disabled, the viewer SHALL NOT write any view-mode record and SHALL seed startup state exactly as it does today (INI seeds and built-in defaults).

#### Scenario: Flag enabled
- **WHEN** `multidocviewer.ini` contains `[Viewer] AutosaveViewMode=true` and the user changes a view mode
- **THEN** the new state is recorded in the shared user store

#### Scenario: Flag absent
- **WHEN** `multidocviewer.ini` contains no `[Viewer] AutosaveViewMode` key
- **THEN** no view-mode record is written or read, and startup state comes from the existing INI seeds and defaults

### Requirement: Recorded state

With autosave enabled, the viewer SHALL record exactly these state bits: the display mode (paged vs. continuous), the page presentation (single, double, or double with cover), and the fit mode (manual, fit-to-page, or fit-to-width). The viewer MUST NOT record the current page, manual zoom level, rotation, scroll position, or sidebar state.

#### Scenario: Mode change is recorded
- **WHEN** autosave is enabled and the user toggles the display mode, cycles the page presentation, or cycles the fit mode
- **THEN** the corresponding recorded bit is updated to the new value

#### Scenario: Zoom change is not recorded
- **WHEN** autosave is enabled and the user zooms manually or rotates the document
- **THEN** the stored record is unchanged

### Requirement: Timely persistence

The viewer SHALL persist a recorded view-mode change promptly (coalescing rapid changes) so that it survives the host process exiting, without a clean-shutdown hook. The write SHALL reuse the shared user store's atomic-write and write-only-on-change guarantees, and a failed write MUST NOT crash the viewer or lose already-stored favorites.

#### Scenario: Change survives an abrupt exit
- **WHEN** the user cycles the fit mode and the host process exits before any further interaction
- **THEN** the next start reads the new fit mode from the store, provided the write completed

#### Scenario: No redundant writes
- **WHEN** the recorded state equals the last saved state
- **THEN** the store file is not rewritten

### Requirement: Storage in the shared user store

The record SHALL be stored in the same shared JSON user store as favorites, using the same directory resolution (plugin directory when writable, user configuration directory otherwise) and atomic replacement. The plugin's `multidocviewer.ini` MUST NOT be written at runtime. The store format SHALL stay backward compatible: a store file written before this feature MUST still load its favorites, and a store written by this feature MUST still load in older builds without losing favorites.

#### Scenario: Record lives next to favorites
- **WHEN** favorites are stored in `multidocviewer-favorites.json` in a writable directory
- **THEN** the view-mode record is carried in that same file and written atomically with it

#### Scenario: Older store file loads
- **WHEN** the store file contains favorites but no view-mode record and autosave is enabled
- **THEN** favorites load normally and startup state falls back to the INI seeds/defaults

### Requirement: Restore takes precedence over INI seeds

On load, when autosave is enabled and a valid saved record exists, the viewer SHALL seed the display mode, page presentation, and fit mode from the record; the record takes precedence over the static `[Viewer] PagedMode` and `[Viewer] FitMode` seeds. The saved fit mode SHALL apply on every document open in the session, and the saved display mode SHALL apply to each new lister window, while runtime toggles still win for the lifetime of that window. A missing, malformed, or out-of-range record SHALL fall back to the INI seeds and defaults as if no record existed.

#### Scenario: Restored paged state
- **WHEN** the record says paged mode + double with cover + fit-to-width and the user opens a document
- **THEN** the viewer starts in paged mode with double-with-cover presentation and fit-to-width, and the toolbar shows those states

#### Scenario: Record beats a conflicting INI seed
- **WHEN** `[Viewer] PagedMode=false` but the record says paged mode, with autosave enabled
- **THEN** the viewer starts in paged mode

#### Scenario: Malformed record
- **WHEN** the record contains an unknown presentation or fit-mode value
- **THEN** that bit falls back to the INI seed or built-in default and the viewer still opens the document

### Requirement: Global record scope

The record SHALL be global: one record shared by all documents, not keyed per document path. A view-mode change made while any document is open updates the single record used by subsequent loads of any document.

#### Scenario: Different document, same seed
- **WHEN** the user sets paged mode while reading document A, closes the lister, and opens unrelated document B
- **THEN** document B opens in paged mode from the same single record
