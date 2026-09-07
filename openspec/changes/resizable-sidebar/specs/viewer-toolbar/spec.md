## ADDED Requirements

### Requirement: Sidebar reveals line beginnings

The viewer SHALL align the outline sidebar's tree to the left edge, resetting the tree's horizontal scroll position to its start whenever entries are (re)loaded into the sidebar or an entry is brought into view by selection or navigation (keyboard, mouse, or page-driven highlight sync), so overflowed titles show their beginnings. The viewer SHALL NOT lock horizontal scrolling: the user may still scroll the tree horizontally after such a reset.

#### Scenario: Selection shows line beginning
- **WHEN** the user navigates to an outline entry whose title overflows the sidebar width
- **THEN** the entry is brought into view with the tree's horizontal scroll reset to the left edge, making the beginning of the title visible

#### Scenario: Reload resets to left
- **WHEN** the sidebar repopulates with a new document's outline
- **THEN** the tree's horizontal scroll starts at the left edge

#### Scenario: Manual horizontal scroll retained between reveals
- **WHEN** the user scrolls the tree horizontally with the scrollbar or wheel without triggering an entry reveal
- **THEN** the horizontal position remains where the user placed it

### Requirement: Resizable sidebar width

The viewer SHALL allow the user to resize the outline sidebar's width by dragging its right edge, within a documented minimum of 80 logical pixels and a maximum of half the lister page-area width. The page area SHALL relayout live during the drag so displayed pages immediately use the new width. The chosen width SHALL persist across document loads into the same viewer window and SHALL reset to the initial width for a new viewer window: the `[Viewer] SidebarWidth` value from the plugin's `multidocviewer.ini` (default 180 logical pixels when the key is absent). The viewer SHALL NOT write the width to any settings file at any time.

#### Scenario: Drag to widen
- **WHEN** the user drags the sidebar's right edge to the right
- **THEN** the sidebar widens to follow the pointer (clamped to the bounds), more of each outline title becomes visible, and the page area relayouts to the reduced width in real time

#### Scenario: Drag to narrow
- **WHEN** the user drags the sidebar's right edge to the left above the default width
- **THEN** the sidebar narrows to follow the pointer and the page area widens accordingly, down to the minimum width

#### Scenario: Clamped at bounds
- **WHEN** the user drags past the minimum or maximum width
- **THEN** the sidebar stops at the bound and does not exceed it

#### Scenario: Same-window reload keeps width
- **WHEN** the same viewer window loads a new document
- **THEN** the sidebar, when visible, keeps the resized width

#### Scenario: New window uses configured initial width
- **WHEN** a new viewer window is created
- **THEN** the sidebar starts at the `[Viewer] SidebarWidth` ini value (180 logical pixels when the key is absent)

### Requirement: Sidebar default visibility from INI

The viewer SHALL show the sidebar by default only when the open document has an outline. The initial visibility SHALL be controlled by the `[Viewer] SidebarVisible` value from the plugin's `multidocviewer.ini`: when it is `true`, the sidebar is visible on load for documents that have an outline; when it is absent or `false`, the sidebar starts hidden and the user shows it with the toggle. The setting SHALL NOT affect documents without an outline (no sidebar appears) and SHALL NOT override the user's manual toggle.

#### Scenario: Visible by default from INI
- **WHEN** `multidocviewer.ini` sets `[Viewer] SidebarVisible=true` and the viewer opens a document that has an outline
- **THEN** the sidebar is visible immediately, without the user activating the toggle

#### Scenario: Hidden by default (absent or false)
- **WHEN** `[Viewer] SidebarVisible` is absent or `false` and the viewer opens a document that has an outline
- **THEN** the sidebar starts hidden

#### Scenario: No outline ignores the setting
- **WHEN** `[Viewer] SidebarVisible=true` but the open document exposes no outline
- **THEN** no sidebar appears