## ADDED Requirements

### Requirement: Fit modes track the current view unit in paged mode

When fit-to-page or fit-to-width is active and the viewer is in paged mode, the viewer SHALL recompute the fit zoom against the current view unit whenever a navigation command changes the current unit, so every view unit satisfies the active fit rule independently of the other pages in the document and of the unit that was current when the document opened or last changed viewport.

A view unit SHALL be defined as in `viewer-display-modes`: one page in single-page presentation, two adjacent pages in double page presentation, or a cover page alone followed by two-page pairs in double page with cover presentation. In fit-to-page the zoom SHALL be chosen so the current unit fits entirely within the viewport; in fit-to-width the zoom SHALL be chosen so the current unit's width equals the viewport width.

This requirement applies to paged mode only. In continuous mode the viewer SHALL keep a single document-wide fit zoom and SHALL NOT recompute it against the page that happens to be selected, so the layed-out strip does not shift (see `viewer-scrolling` "Zoom and layout changes preserve the viewport anchor").

#### Scenario: Navigate a mixed-size document in fit-to-page paged mode
- **WHEN** the viewer is in paged mode with fit-to-page active and the document contains pages of different sizes (e.g. portrait and landscape pages), and the user navigates page by page
- **THEN** each newly displayed view unit is scaled to fit entirely within the viewport, regardless of what the previously displayed unit's size was

#### Scenario: Fit-to-width in paged mode fits the current unit
- **WHEN** the viewer is in paged mode with fit-to-width active and navigates to a view unit whose width differs from the widest row in the document
- **THEN** the current unit is scaled so its width equals the viewport width, independent of the document's widest row

#### Scenario: Continuous mode keeps the anchored fit zoom
- **WHEN** the viewer is in continuous mode with fit-to-page or fit-to-width active and navigates between pages
- **THEN** the zoom does not change during navigation and the layout does not shift