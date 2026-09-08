## ADDED Requirements

### Requirement: PageUp/PageDown advance one view unit per step in paged mode

In paged mode, the PageUp/PageDown commands SHALL advance the view by one view unit per key press when the current view unit fits within the viewport at the active zoom, and SHALL scroll within the current view unit when it overflows the viewport vertically.

A view unit SHALL be defined as in `viewer-display-modes`: one page in single-page presentation, two adjacent pages in double page presentation, or a cover page alone followed by two-page pairs in double page with cover presentation.

When the current view unit fits the viewport vertically (either exactly, or with an overflow no larger than the small overlap band the viewer keeps between consecutive blocks), a single PageDown press SHALL advance to the next view unit and a single PageUp press SHALL return to the previous view unit, with the scroll position reset to the top. When the current view unit overflows the viewport by more than that overlap band, the first press SHALL scroll the visible part down/up by one screenful toward the overflow; once the unit is fully visible or already scrolled to its end, the next press SHALL advance.

This applies to paged mode only; in continuous mode PageUp/PageDown SHALL scroll by one screenful and may land mid-page (see `viewer-scrolling`).

#### Scenario: PageDown on a unit that fits advances one unit
- **WHEN** the viewer is in paged mode, fit-to-page is active (so the current view unit fits the viewport), and the user presses PageDown
- **THEN** the viewer advances to the next view unit with the scroll position reset, without a preliminary scroll step within the current unit

#### Scenario: PageUp on a unit that fits returns one unit
- **WHEN** the viewer is in paged mode, the current view unit fits the viewport, and the user presses PageUp
- **THEN** the viewer returns to the previous view unit with the scroll position reset, without a preliminary scroll step

#### Scenario: PageDown on a unit taller than the viewport scrolls first
- **WHEN** the viewer is in paged mode and the current view unit is taller than the viewport (e.g. manual zoom or fit-to-width on a tall page) so it overflows vertically
- **THEN** the first PageDown press scrolls the visible part of the unit by one screenful, and only once the overflow is exhausted does the next PageDown advance to the following view unit