## ADDED Requirements

### Requirement: Page-presentation three-state control

The toolbar SHALL provide a page-presentation control that cycles through three states: single page, double page, and double page with cover. The control's displayed state SHALL reflect which presentation is currently active. It SHALL be placed between the display-mode toggle button and the fit-mode button. Activating it SHALL cycle to the next presentation state, independent of the paged/continuous mode.

#### Scenario: Cycle to double page
- **WHEN** the user clicks the page-presentation control while single-page presentation is active
- **THEN** the viewer switches to double page presentation and the control updates to show the double-page state

#### Scenario: Cycle to double page with cover
- **WHEN** the user clicks the page-presentation control while double page presentation is active
- **THEN** the viewer switches to double page with cover presentation and the control updates to show the cover state

#### Scenario: Cycle back to single page
- **WHEN** the user clicks the page-presentation control while double page with cover presentation is active
- **THEN** the viewer switches back to single page presentation and the control updates to show the single-page state

#### Scenario: Presentation preserved across mode toggle
- **WHEN** the user changes paged/continuous mode while any double-page presentation state is active
- **THEN** the presentation control stays in its current state and the mode switch proceeds with the double-page presentation intact
