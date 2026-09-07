## MODIFIED Requirements

### Requirement: Paged display mode

The viewer SHALL support a paged display mode in which a single view unit is visible at a time, scaled to fit the viewport. A view unit is one page in single-page presentation, two adjacent pages in double page presentation, or a cover page alone followed by two-page pairs in double page with cover presentation (see `viewer-display-modes/two-page-view`). Mouse-wheel input in paged mode SHALL navigate strictly from one view unit to the next or previous view unit.

#### Scenario: Paged mode shows one page
- **WHEN** the viewer is in paged mode
- **THEN** the current view unit is visible inside the viewport, scaled to fit

#### Scenario: Paged mode wheel jumps strictly page-to-page
- **WHEN** the viewer is in paged mode and the user scrolls the mouse wheel
- **THEN** the viewer advances to the next view unit on a downward wheel and to the previous view unit on an upward wheel, with no in-between scroll positions

#### Scenario: Paged mode keyboard next/prev resets scroll
- **WHEN** the viewer is in paged mode and the user issues a keyboard "next page" or "previous page" command
- **THEN** the displayed view unit changes and the scroll position is reset to the top

### Requirement: Continuous display mode

The viewer SHALL support a continuous display mode in which view units are rendered sequentially and the user scrolls vertically through them. A view unit is one page in single-page presentation, two adjacent pages in double page presentation, or a cover page alone followed by two-page pairs in double page with cover presentation (see `viewer-display-modes/two-page-view`). Mouse-wheel input in continuous mode SHALL scroll smoothly so the user can see the border between adjacent view units during navigation.

#### Scenario: Continuous mode lays pages vertically
- **WHEN** the viewer is in continuous mode
- **THEN** consecutive view units are rendered vertically with a small gap between them

#### Scenario: Continuous mode wheel scrolls smoothly
- **WHEN** the viewer is in continuous mode and the user scrolls the mouse wheel
- **THEN** the viewport scrolls smoothly across view-unit boundaries and the border between the current and adjacent view unit is visible during the scroll

#### Scenario: Continuous mode keyboard next/prev preserves viewport
- **WHEN** the viewer is in continuous mode and the user issues a keyboard "next page" or "previous page" command
- **THEN** the document advances by exactly one view unit and the vertical scroll position is left unchanged, so the same viewport slice continues to show the next view unit (see `viewer-navigation`)

### Requirement: Toggle display mode via keyboard

The viewer SHALL provide a keyboard command that toggles between paged and continuous mode without losing the current page index. The page-presentation setting is preserved across this toggle.

#### Scenario: Switch from paged to continuous
- **WHEN** the viewer is in paged mode and the toggle command is issued
- **THEN** the viewer switches to continuous mode, keeps the current page index, and re-renders

#### Scenario: Switch from continuous to paged
- **WHEN** the viewer is in continuous mode and the toggle command is issued
- **THEN** the viewer switches to paged mode showing the view unit that was at the top of the current viewport

### Requirement: Default mode is paged

The viewer SHALL start in paged mode with single-page presentation when a document is opened.

#### Scenario: Fresh load
- **WHEN** a document is opened
- **THEN** the viewer is in paged mode on page 1 with single-page presentation
