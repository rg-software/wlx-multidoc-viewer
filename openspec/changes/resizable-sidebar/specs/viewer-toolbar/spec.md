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

The viewer SHALL allow the user to resize the outline sidebar's width by dragging its right edge, within a documented minimum of 80 logical pixels and a maximum of half the lister page-area width. The page area SHALL relayout live during the drag so displayed pages immediately use the new width. The chosen width SHALL persist across document loads into the same viewer window and SHALL reset to the default width (180 logical pixels) when a new viewer window is created; it SHALL NOT be written to any settings file.

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

#### Scenario: New window resets width
- **WHEN** a new viewer window is created
- **THEN** the sidebar starts at the default 180 logical pixel width