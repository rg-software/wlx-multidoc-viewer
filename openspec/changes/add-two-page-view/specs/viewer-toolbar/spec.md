## ADDED Requirements

### Requirement: Page-presentation toggle

The toolbar SHALL provide a two-page presentation toggle button whose pressed state reflects whether two-page presentation is active. It SHALL be placed between the display-mode toggle button and the fit-mode button. Activating it SHALL switch between one-page and two-page presentation exactly as the corresponding keyboard command does, independent of the paged/continuous mode.

#### Scenario: Toggle presentation
- **WHEN** the user clicks the page-presentation toggle button
- **THEN** the viewer switches between one-page and two-page presentation and the button's pressed state updates to match

#### Scenario: Presentation preserved across mode toggle
- **WHEN** the user changes paged/continuous mode while two-page presentation is active
- **THEN** the presentation button stays in the two-page pressed state and the mode switch proceeds with two-page presentation intact
