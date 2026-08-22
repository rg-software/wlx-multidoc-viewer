# Viewer Info Panel Specification

## Purpose

Defines the top-of-viewport info panel that surfaces the viewer's live state — current page index, total page count, continuous-mode status, and fit-to-page status — to the user. The panel sits above the page area on both Windows and Linux viewers and is the canonical source of these state values (the host-facing `ListSendCommand(lc_copy, ...)` payload mirrors the current/total portion).

## Requirements

### Requirement: Info panel is always visible while a document is open

The viewer SHALL display the info panel at the top of the viewport whenever a document is loaded. When no document is open, the panel SHALL remain visible but display placeholder text (e.g. `- / -`).

#### Scenario: Panel visible after load
- **WHEN** a document is successfully loaded
- **THEN** the info panel is rendered at the top of the viewport

#### Scenario: Panel placeholder when no document
- **WHEN** no document is open
- **THEN** the info panel is visible but shows placeholder values (e.g. `- / -`)

### Requirement: Info panel shows current and total page

The info panel SHALL display the current page index and the total page count of the open document in the form `current / total`.

#### Scenario: Format on load
- **WHEN** a document finishes loading
- **THEN** the panel reads `"1 / <total>"`

#### Scenario: Format on navigation
- **WHEN** the user navigates to a different page
- **THEN** the panel updates to reflect the new current page

### Requirement: Info panel shows continuous-mode status

The info panel SHALL display whether continuous display mode is active. The status reads `Continuous: ON` when continuous mode is active and `Continuous: OFF` when paged mode is active.

#### Scenario: Continuous on
- **WHEN** the viewer is in continuous mode
- **THEN** the panel shows `Continuous: ON`

#### Scenario: Continuous off
- **WHEN** the viewer is in paged mode
- **THEN** the panel shows `Continuous: OFF`

#### Scenario: Status updates on toggle
- **WHEN** the user toggles between paged and continuous mode
- **THEN** the panel reflects the new state on the next paint

### Requirement: Info panel shows current fit mode

The info panel SHALL display the current fit mode as `Fit: Page` (fit-to-page), `Fit: Width` (fit-to-width), or `Zoom: <pct>%` (manual zoom, `<pct>` rounded to the nearest integer — e.g. `Zoom: 100%` at 1.0x, `Zoom: 150%` at 1.5x).

#### Scenario: Fit-to-page active
- **WHEN** the viewer is in the fit-to-page state of the fit-mode cycle
- **THEN** the panel shows `Fit: Page`

#### Scenario: Fit-to-width active
- **WHEN** the viewer is in the fit-to-width state of the fit-mode cycle
- **THEN** the panel shows `Fit: Width`

#### Scenario: Manual zoom active
- **WHEN** the viewer is in manual zoom mode at 200% zoom
- **THEN** the panel shows `Zoom: 200%`

#### Scenario: Status updates on fit-mode cycle
- **WHEN** the user invokes the fit-mode cycle and the resulting state changes
- **THEN** the panel reflects the new state on the next paint

### Requirement: Info panel does not occlude the page area

The info panel SHALL be sized so it does not cover the page rendering area; the page is rendered in the remaining viewport below the panel.

#### Scenario: Panel leaves room for the page
- **WHEN** the info panel is visible
- **THEN** the page render area is the viewport height minus the panel height, and the page aspect-ratio and fit-mode behaviors are computed against this reduced area