## Purpose

Lets users mark individual pages of a document as favorites, return to them from the sidebar, and have those marks persist per document across sessions, with identical behavior on the Win32 (Windows) and Qt (Linux) viewers.

## ADDED Requirements

### Requirement: Toggle the current page's favorite status
The viewer SHALL provide an action that marks the current page as a favorite, or un-marks it if it already is one. The action SHALL be reachable as a toolbar button and as a keyboard shortcut (`Ctrl+B`) on both platforms. When a page is marked, the viewer SHALL capture the outline heading of the reading position, if any, as the favorite's label. Marking a page that is already a favorite SHALL NOT create a duplicate; instead the repeated action SHALL remove it.

#### Scenario: Add a favorite via toolbar
- **WHEN** the current page is not a favorite and the user activates the favorites action
- **THEN** the page becomes a favorite with the outline heading of that page, if any, as its label

#### Scenario: Toggle removes an existing favorite
- **WHEN** the current page is a favorite and the user activates the favorites action again
- **THEN** the page is no longer a favorite and no duplicate entry remains for it

#### Scenario: Keyboard shortcut matches the toolbar
- **WHEN** the user presses `Ctrl+B` on either platform
- **THEN** the current page's favorite status toggles exactly as it does with the toolbar button

#### Scenario: Label captured from outline heading
- **WHEN** a page whose position falls under an outline heading is marked as a favorite
- **THEN** the favorite's label is that heading's title

### Requirement: Persist favorites across sessions
The viewer SHALL store favorites in one shared JSON file so they survive closing the lister window and reopening the plugin. Favorites SHALL be keyed by the document's absolute path so distinct documents never share favorites. The viewer SHALL NOT rewrite the store when nothing changed, and SHALL write it atomically so an interrupted save never corrupts existing favorites. The plugin's existing `multidocviewer.ini` SHALL remain read-only; favorites SHALL NOT be stored in it.

#### Scenario: Favorites survive reopen
- **WHEN** the user marks one or more pages as favorites, closes the document, then reopens the same file later
- **THEN** the marked pages are still favorites with their labels

#### Scenario: Distinct files keep separate favorites
- **WHEN** the user has favorites for two different documents
- **THEN** opening one document shows and edits only its own favorites, never the other's

#### Scenario: Interrupted save preserves previous favorites
- **WHEN** a save of the favorites store is interrupted mid-write
- **THEN** the previously saved favorites remain intact and loadable on the next start

#### Scenario: No-op does not write
- **WHEN** favorites are toggled on and off again so the in-memory set equals the last saved set
- **THEN** the store file is not rewritten unnecessarily

### Requirement: Storage location resolution
The viewer SHALL store the favorites file next to the plugin module when that directory is writable, and in the OS user configuration directory otherwise. The location SHALL be resolved at most once per viewer process and reused for all reads and writes. A failed write SHALL NOT lose in-memory favorites and SHALL NOT crash the viewer; the viewer SHALL retain the data in memory and retry on the next change.

#### Scenario: Writable plugin directory
- **WHEN** the directory containing the plugin is writable and the viewer saves favorites
- **THEN** the favorites file is created in that directory

#### Scenario: Read-only plugin directory
- **WHEN** the directory containing the plugin is not writable and the viewer saves favorites
- **THEN** the favorites file is created in the OS user configuration directory instead

#### Scenario: Write failure does not crash
- **WHEN** a write to the resolved favorites file fails (e.g. the volume becomes unavailable)
- **THEN** the viewer keeps the favorites in memory, continues working, and retries saving on a later change

### Requirement: Jump to a favorite
Activating a favorite entry in the sidebar SHALL navigate the viewer to that favorite's page. In paged mode the viewer SHALL display the page; in continuous mode the viewer SHALL scroll the page's area into view, matching the navigation behavior of outline entries.

#### Scenario: Activate favorite in paged mode
- **WHEN** the user activates a favorite entry in the sidebar while in paged mode
- **THEN** the viewer displays that favorite's page

#### Scenario: Activate favorite in continuous mode
- **WHEN** the user activates a favorite entry in the sidebar while in continuous mode
- **THEN** the viewer scrolls to that favorite's page

#### Scenario: Favorite out of range on reopen
- **WHEN** a favorite's page is outside the current document's page range (e.g. after a reflow changed the page count)
- **THEN** the activation has no effect and the current page is unchanged

### Requirement: Highlight the current position among favorites
While the sidebar is visible, the viewer SHALL highlight the favorite entry whose page equals the currently displayed page, and SHALL update it when the position changes by any means. When the current page is not a favorite, no favorite entry SHALL be highlighted. Favorites rows SHALL use exact-page matching; they SHALL NOT participate in the outline's deepest-entry-at-or-before highlight policy.

#### Scenario: Favorite page highlighted on arrival
- **WHEN** the viewer navigates to a page that is a favorite
- **THEN** that favorite's row in the sidebar becomes highlighted

#### Scenario: Non-favorite page clears the highlight
- **WHEN** the viewer navigates to a page that is not a favorite
- **THEN** no favorite row is highlighted

### Requirement: Favorites section in the sidebar
Whenever the open document has at least one favorite, the sidebar SHALL show a dedicated, collapsible Favorites section consisting of a flat, non-hierarchical list of the document's favorite entries, displayed directly below the outline entries. Favorites SHALL be presented in ascending page order. When the document has no favorites, the section SHALL not appear.

#### Scenario: Section below outline
- **WHEN** the sidebar is visible and the open document has both an outline and favorites
- **THEN** the outline entries appear first and the flat Favorites section appears directly below them

#### Scenario: Section hidden when empty
- **WHEN** the sidebar is visible and the open document has no favorites
- **THEN** no Favorites section is shown and the sidebar shows only the outline, if any

#### Scenario: Collapsible section
- **WHEN** the user collapses the Favorites section header
- **THEN** the favorite entries hide and the header remains, and expanding it restores the entries