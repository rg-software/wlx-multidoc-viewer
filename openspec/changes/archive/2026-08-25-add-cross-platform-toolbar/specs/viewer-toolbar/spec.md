# viewer-toolbar delta

## Purpose

Provides the viewer's on-window chrome: a control strip docked inside the viewer window so users can navigate, zoom, rotate, switch display modes, find text, and print without memorizing keyboard shortcuts, plus a toggleable outline sidebar — with identical behavior on Windows and Linux.

## ADDED Requirements

### Requirement: Toolbar presence and layout
The viewer SHALL display a toolbar docked along the top edge of the viewer window whenever it displays a document, on both platforms. The toolbar SHALL occupy a fixed-height strip that reduces the page display area without overlapping it, and SHALL remain docked and fully usable when the window is resized.

#### Scenario: Toolbar appears with document
- **WHEN** the viewer opens a document
- **THEN** a toolbar is visible at the top of the viewer window and the page area renders below it

#### Scenario: Toolbar survives resize
- **WHEN** the user resizes the viewer window
- **THEN** the toolbar stays docked at the top spanning the window width and all controls remain reachable

### Requirement: Navigation controls
The toolbar SHALL provide previous-page and next-page buttons, an editable current-page number box, and a read-only label showing the total page count in the form "/ N". Committing a value in the page box (Enter or focus leave) SHALL navigate to that page, clamped to the valid page range. Each navigation button SHALL be disabled when no adjacent page exists.

#### Scenario: Next page
- **WHEN** the current page is less than the last page and the user clicks the next-page button
- **THEN** the viewer displays the following page

#### Scenario: Jump to typed page
- **WHEN** the user types a valid page number into the page box and presses Enter
- **THEN** the viewer displays that page

#### Scenario: Out-of-range page number
- **WHEN** the user commits a number outside 1..N
- **THEN** the viewer navigates to the nearest valid page

#### Scenario: Boundary disabling
- **WHEN** the viewer is on the first (or last) page
- **THEN** the previous-page (or next-page) button is disabled

### Requirement: Display mode toggle
The toolbar SHALL provide a continuous-mode toggle button whose pressed state reflects whether continuous scrolling is active. Activating it SHALL switch between paged and continuous mode exactly as the keyboard shortcut does.

#### Scenario: Toggle mode
- **WHEN** the user clicks the mode toggle button
- **THEN** the viewer switches between paged and continuous mode and the button's pressed state updates to match

### Requirement: Fit-mode cycling button
The toolbar SHALL provide a single fit-mode button that cycles through manual zoom, fit-to-page, and fit-to-width on each activation. The button icon SHALL always reflect the currently active fit mode, including when the mode was changed by other means such as keyboard shortcuts.

#### Scenario: Cycle modes
- **WHEN** the user clicks the fit-mode button repeatedly
- **THEN** the fit mode advances through manual → fit-to-page → fit-to-width → manual, and the icon changes accordingly

#### Scenario: External change reflected
- **WHEN** the fit mode is changed via keyboard while the toolbar is visible
- **THEN** the fit-mode button icon updates to the new mode

### Requirement: Rotation controls
The toolbar SHALL provide rotate-left and rotate-right buttons that rotate the displayed pages 90° counter-clockwise or clockwise respectively, equivalent to the existing rotation behavior.

#### Scenario: Rotate right
- **WHEN** the user clicks rotate-right
- **THEN** all displayed pages render rotated 90° clockwise from their previous orientation

### Requirement: Zoom controls
The toolbar SHALL provide zoom-in and zoom-out buttons that adjust the zoom level using the same steps and limits as the keyboard zoom shortcuts.

#### Scenario: Zoom in
- **WHEN** the user clicks zoom-in below the maximum zoom
- **THEN** the page content enlarges by one standard zoom step

### Requirement: Text-find controls
The toolbar SHALL provide a find text box, previous-match and next-match buttons, and a match-case toggle, presenting the text-search capability described in `viewer-text-search`.

#### Scenario: Find controls present
- **WHEN** a searchable document is open
- **THEN** the toolbar shows the find box, match navigation buttons, and match-case toggle

### Requirement: Print control
The toolbar SHALL provide a print button that opens the system print dialog as described in `viewer-printing`.

#### Scenario: Open print dialog
- **WHEN** the user clicks the print button
- **THEN** the platform-native print dialog opens

### Requirement: Toolbar state synchronization
The toolbar SHALL reflect viewer state changes regardless of their origin: navigating by keyboard or mouse updates the page box, zoom changes update enabled state of zoom buttons, and mode or fit changes update the corresponding button visuals. Toolbar-originated actions and external state changes MUST NOT diverge.

#### Scenario: Keyboard navigation reflected
- **WHEN** the user moves to another page with keyboard shortcuts
- **THEN** the page box shows the new current page

### Requirement: Keyboard focus neutrality
When keyboard focus is not inside a toolbar edit box, all existing viewer keyboard shortcuts SHALL continue to work unchanged. While focus is inside an editable toolbar control, typed characters SHALL go to that control and MUST NOT trigger viewer shortcuts; after the user leaves the control, shortcuts work again.

#### Scenario: Typing does not trigger shortcuts
- **WHEN** the user types digits into the page box
- **THEN** no viewer shortcut actions fire while typing

#### Scenario: Shortcuts restored after leaving
- **WHEN** the user moves focus away from a toolbar edit box back to the page area
- **THEN** keyboard navigation shortcuts operate normally again

### Requirement: Cross-platform parity
Both the Windows and Linux viewers SHALL expose the same toolbar control set with the same behaviors; platform differences are limited to native widget appearance.

#### Scenario: Same controls on both platforms
- **WHEN** a given document type is opened on Windows and on Linux
- **THEN** both viewers offer every control defined in this capability with matching behavior

### Requirement: Document-dependent availability
Controls that require a text layer (find controls) SHALL be disabled for documents whose format provides none. Controls that require a document SHALL be disabled when no document is open.

#### Scenario: Non-searchable format
- **WHEN** the viewer opens a document whose engine provides no text layer
- **THEN** the find box, match navigation buttons, and match-case toggle are disabled while other controls work

### Requirement: Copy control placeholder
The toolbar SHALL include a copy button reserved for copying the currently selected text to the clipboard. Text selection does not yet exist in the plugin (it is owned by a separate future change), so until that capability is delivered the copy button MUST remain permanently disabled and MUST NOT perform any fallback action such as copying page text.

#### Scenario: Placeholder state
- **WHEN** any document is open
- **THEN** the copy button is visible but disabled, and activating it has no effect

### Requirement: Outline sidebar availability
The viewer SHALL provide a toggleable sidebar docked at the left edge of the window listing the open document's embedded outline (table of contents) whenever the document exposes one. Entries SHALL reflect the outline's hierarchy, and activating an entry SHALL navigate to its target page. The sidebar's visibility SHALL persist across resizes without overlapping the page area. When the document exposes no outline, the viewer MUST NOT show a sidebar and its toggle control MUST be unavailable.

#### Scenario: Toggle reveals outline
- **WHEN** a document with an embedded outline is open and the user activates the sidebar toggle
- **THEN** a left sidebar appears showing the hierarchical table of contents without shrinking the toolbar or info panel

#### Scenario: Entry navigation
- **WHEN** the user activates an outline entry
- **THEN** the viewer navigates to that entry's target page

#### Scenario: Document without outline
- **WHEN** the viewer opens a document whose engine provides no outline
- **THEN** no sidebar exists and the toggle control is hidden or disabled

### Requirement: Outline position sync
While the sidebar is visible, the viewer SHALL highlight the outline entry corresponding to the currently displayed reading position and update it when the position changes by any means (sidebar click, keyboard, scroll). Collapsed entries SHALL auto-expand along the path to the highlighted entry.

#### Scenario: Keyboard navigation updates selection
- **WHEN** the sidebar shows the outline and the user changes pages with keyboard shortcuts
- **THEN** the entry matching the new reading position becomes highlighted

### Requirement: DPI-aware toolbar sizing
Toolbar height, control sizes, icons, and the outline sidebar's dimensions SHALL scale with the system DPI scale factor so the toolbar remains legible and proportionate on high-DPI displays, consistent with the rest of the viewer.

#### Scenario: High-DPI display
- **WHEN** the viewer runs at 200% system DPI
- **THEN** the toolbar and its icons render at twice the base pixel dimensions without blurring
