## Purpose

Lets users switch between paged mode (one page fills the viewport) and continuous mode (pages stack vertically with smooth scrolling) to suit their reading preference.

## ADDED Requirements

### Requirement: Paged mode
The viewer SHALL support a paged mode where exactly one page is visible at a time, filling the viewport.

#### Scenario: Paged mode displays single page
- **WHEN** the viewer is in paged mode
- **THEN** only the current page is visible, scaled to fit the viewport

#### Scenario: Paged mode scroll advances page
- **WHEN** the user scrolls past the bottom of the current page in paged mode
- **THEN** the viewer advances to the next page

### Requirement: Continuous mode
The viewer SHALL support a continuous mode where pages are rendered sequentially and the user scrolls vertically through them.

#### Scenario: Continuous mode shows multiple pages
- **WHEN** the viewer is in continuous mode
- **THEN** consecutive pages are rendered vertically with a small gap between them

#### Scenario: Continuous mode smooth scroll
- **WHEN** the user scrolls in continuous mode
- **THEN** the viewport scrolls smoothly across page boundaries

### Requirement: Mode toggle
The viewer SHALL allow switching between paged and continuous mode via a keyboard shortcut.

#### Scenario: Toggle to continuous mode
- **WHEN** the user presses Ctrl+N (or platform equivalent)
- **THEN** the viewer switches to continuous mode and re-renders

#### Scenario: Toggle to paged mode
- **WHEN** the user presses Ctrl+N while in continuous mode
- **THEN** the viewer switches to paged mode showing the current page
