## Purpose

Defines the viewer's two display modes for paging through a document — paged (one page at a time, mouse wheel jumps strictly page-to-page) and continuous (pages stacked vertically, mouse wheel scrolls smoothly across page boundaries) — and how the user switches between them. Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## ADDED Requirements

### Requirement: Paged display mode

The viewer SHALL support a paged display mode in which exactly one page is visible at a time, scaled to fit the viewport. Mouse-wheel input in paged mode SHALL navigate strictly from one page to the next or previous page.

#### Scenario: Paged mode shows one page
- **WHEN** the viewer is in paged mode
- **THEN** only the current page is visible inside the viewport, scaled to fit

#### Scenario: Paged mode wheel jumps strictly page-to-page
- **WHEN** the viewer is in paged mode and the user scrolls the mouse wheel
- **THEN** the viewer advances to the next page on a downward wheel and to the previous page on an upward wheel, with no in-between scroll positions

#### Scenario: Paged mode keyboard next/prev resets scroll
- **WHEN** the viewer is in paged mode and the user issues a keyboard "next page" or "previous page" command
- **THEN** the displayed page changes and the scroll position is reset to the top

### Requirement: Continuous display mode

The viewer SHALL support a continuous display mode in which pages are rendered sequentially and the user scrolls vertically through them. Mouse-wheel input in continuous mode SHALL scroll smoothly so the user can see the border between adjacent pages during navigation.

#### Scenario: Continuous mode lays pages vertically
- **WHEN** the viewer is in continuous mode
- **THEN** consecutive pages are rendered vertically with a small gap between them

#### Scenario: Continuous mode wheel scrolls smoothly
- **WHEN** the viewer is in continuous mode and the user scrolls the mouse wheel
- **THEN** the viewport scrolls smoothly across page boundaries and the border between the current and adjacent page is visible during the scroll

#### Scenario: Continuous mode keyboard next/prev preserves viewport
- **WHEN** the viewer is in continuous mode and the user issues a keyboard "next page" or "previous page" command
- **THEN** the document advances by exactly one page and the vertical scroll position is left unchanged, so the same viewport slice continues to show the next page (see `viewer-navigation`)

### Requirement: Toggle display mode via keyboard

The viewer SHALL provide a keyboard command that toggles between paged and continuous mode without losing the current page index.

#### Scenario: Switch from paged to continuous
- **WHEN** the viewer is in paged mode and the toggle command is issued
- **THEN** the viewer switches to continuous mode, keeps the current page index, and re-renders

#### Scenario: Switch from continuous to paged
- **WHEN** the viewer is in continuous mode and the toggle command is issued
- **THEN** the viewer switches to paged mode showing the page that was at the top of the current viewport

### Requirement: Default mode is paged

The viewer SHALL start in paged mode when a document is opened.

#### Scenario: Fresh load
- **WHEN** a document is opened
- **THEN** the viewer is in paged mode on page 1
