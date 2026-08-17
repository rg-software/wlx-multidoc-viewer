## Purpose

Provides the Qt6 user interface that displays rendered document pages and exposes navigation, zoom, and document info controls to the user.

## ADDED Requirements

### Requirement: Page display
The viewer widget SHALL display the current rendered page in a scrollable area that fills the available space below the toolbar.

#### Scenario: Display rendered page
- **WHEN** a document is loaded and the first page is rendered
- **THEN** the page image SHALL be displayed centered in the viewer area, scaled to fit the available width by default

#### Scenario: Window resize
- **WHEN** the Double Commander preview pane is resized
- **THEN** the viewer widget SHALL reflow and re-display the current page to fit the new dimensions

### Requirement: Page navigation toolbar
The viewer widget SHALL include a toolbar with Previous Page, Next Page, First Page, and Last Page actions.

#### Scenario: Next page
- **WHEN** the user clicks the Next Page button and is not on the last page
- **THEN** the viewer SHALL advance to the next page and display it

#### Scenario: Previous page
- **WHEN** the user clicks the Previous Page button and is not on the first page
- **THEN** the viewer SHALL go back to the previous page and display it

#### Scenario: First page
- **WHEN** the user clicks the First Page button
- **THEN** the viewer SHALL display page 1

#### Scenario: Last page
- **WHEN** the user clicks the Last Page button
- **THEN** the viewer SHALL display the last page

#### Scenario: Already on last page
- **WHEN** the user clicks Next Page while on the last page
- **THEN** the viewer SHALL stay on the last page (no wrap-around)

#### Scenario: Already on first page
- **WHEN** the user clicks Previous Page while on the first page
- **THEN** the viewer SHALL stay on the first page

### Requirement: Page counter display
The toolbar SHALL display a page counter in the format `current/total` (e.g. `3/42`).

#### Scenario: Page counter updates
- **WHEN** the user navigates to page 5 of a 42-page document
- **THEN** the counter SHALL display `5/42`

#### Scenario: Single-page document
- **WHEN** a single-page image is opened
- **THEN** the counter SHALL display `1/1`

### Requirement: Zoom controls
The toolbar SHALL provide Zoom In, Zoom Out, and Original Size (100%) actions.

#### Scenario: Zoom in
- **WHEN** the user clicks Zoom In
- **THEN** the zoom level SHALL increase by 25% and the page SHALL re-render at the new zoom

#### Scenario: Zoom out
- **WHEN** the user clicks Zoom Out
- **THEN** the zoom level SHALL decrease by 25% and the page SHALL re-render at the new zoom

#### Scenario: Original size
- **WHEN** the user clicks Original Size
- **THEN** the zoom level SHALL reset to 100% (1 point = 1 pixel)

#### Scenario: Minimum zoom
- **WHEN** the user is at the minimum zoom level and clicks Zoom Out
- **THEN** the zoom SHALL NOT go below 10%

#### Scenario: Maximum zoom
- **WHEN** the user is at the maximum zoom level and clicks Zoom In
- **THEN** the zoom SHALL NOT exceed 500%

### Requirement: Fit mode toggle
The toolbar SHALL provide a Fit mode toggle that cycles between Fit to Width and Fit to Page.

#### Scenario: Fit to width
- **WHEN** the user activates Fit to Width mode
- **THEN** the page SHALL be scaled so its width fills the viewer area, and vertical scrolling may be needed

#### Scenario: Fit to page
- **WHEN** the user activates Fit to Page mode
- **THEN** the page SHALL be scaled so the entire page is visible within the viewer area without scrolling

### Requirement: Document info dialog
The toolbar SHALL provide an Info action that opens a dialog showing document metadata.

#### Scenario: Show info
- **WHEN** the user clicks the Info button
- **THEN** a modal dialog SHALL appear displaying available metadata fields (title, author, subject, creator, producer, dates) — empty fields are omitted

#### Scenario: No metadata available
- **WHEN** the Info button is clicked for a document with no metadata
- **THEN** the dialog SHALL display a message indicating no information is available

### Requirement: Keyboard shortcuts
The viewer widget SHALL support the following keyboard shortcuts:

| Action | Shortcut |
|--------|----------|
| Next page | Right arrow, Page Down |
| Previous page | Left arrow, Page Up |
| First page | Home |
| Last page | End |
| Zoom in | Ctrl+= |
| Zoom out | Ctrl+- |
| Fit to width | Ctrl+M |
| Go to page | Ctrl+G (opens input dialog) |

#### Scenario: Keyboard navigation
- **WHEN** the viewer has focus and the user presses Right arrow
- **THEN** the viewer SHALL advance to the next page

#### Scenario: Go to page dialog
- **WHEN** the user presses Ctrl+G
- **THEN** a dialog SHALL prompt for a page number, and upon confirmation navigate to that page
