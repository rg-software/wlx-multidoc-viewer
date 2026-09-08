## MODIFIED Requirements

### Requirement: Fit modes in double-page presentation

In a double-page presentation state, fit-to-page SHALL target the whole current unit rather than a single page: the current unit fits inside the viewport, and manual zoom continues to apply to the whole unit.

In paged mode, fit-to-width SHALL target the **current** unit's width rather than the document-wide maximum: the zoom is computed so the current unit (a singleton cover or trailing odd page, or a full spread) fills the viewport width. Because the fit re-targets on every navigation command in paged mode (see `viewer-zoom` "Fit modes track the current view unit in paged mode"), moving from a singleton to a spread re-fits the spread to the viewport width, so the spread never overflows horizontally after navigation.

In continuous mode, fit-to-width SHALL target the two-page spread rather than a single page: the zoom is computed so the width of the widest combined unit in the document equals the viewport width, so a singleton unit (the cover page, or a trailing odd page) does not collapse the fit onto one page and the spread that follows is guaranteed to fit horizontally without overflow.

#### Scenario: Fit unit to viewport
- **WHEN** the viewer is in a double-page presentation and fit-to-page is active
- **THEN** the zoom is computed so the current unit fits inside the viewport

#### Scenario: Fit spread width to viewport
- **WHEN** the viewer is in a double-page presentation and fit-to-width is active and continuous mode is active
- **THEN** the zoom is computed so the width of the document's widest two-page unit equals the viewport width

#### Scenario: Fit-to-width away from a spread
- **WHEN** the viewer is in a double-page presentation, fit-to-width is active, continuous mode is active, and the current unit is a singleton (the cover page or a trailing odd page)
- **THEN** the zoom still uses the two-page spread width as the target, so the singleton renders at a fraction of the viewport width and the next spread fits without overflow

#### Scenario: Paged fit-to-width fits the current unit
- **WHEN** the viewer is in paged mode with a double-page presentation, fit-to-width is active, and the current unit is a singleton (the cover page or a trailing odd page)
- **THEN** the zoom is computed so that singleton's width equals the viewport width

#### Scenario: Paged fit-to-width re-fits the spread on navigation
- **WHEN** the viewer is in paged mode with a double-page presentation, fit-to-width is active, and the user navigates from a singleton cover to the following spread
- **THEN** the zoom is recomputed for the new current unit so the spread width equals the viewport width and the spread does not overflow horizontally