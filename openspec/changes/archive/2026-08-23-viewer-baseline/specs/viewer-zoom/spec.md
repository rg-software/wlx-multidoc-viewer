## Purpose

Defines how the viewer scales the displayed page, including the fit-mode cycle (fit-to-page → fit-to-width → 100%) and manual zoom step. Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## ADDED Requirements

### Requirement: Fit-mode cycle

The viewer SHALL expose a single "fit mode" command that cycles through three states in order: `fit-to-page`, `fit-to-width`, `manual (100%)`, returning to `fit-to-page` on the next press. The current fit state is reported by the info panel (see `viewer-info-panel`).

#### Scenario: First press enters fit-to-page from manual
- **WHEN** the viewer is in any manual zoom state and the fit-mode command is issued
- **THEN** the viewer enters fit-to-page mode and the zoom is recalculated so the page fits entirely inside the viewport at the current window size

#### Scenario: Cycle from fit-to-page to fit-to-width
- **WHEN** the viewer is in fit-to-page mode and the fit-mode command is issued
- **THEN** the viewer enters fit-to-width mode and the zoom is recalculated so the page width equals the viewport width

#### Scenario: Cycle from fit-to-width to manual 100%
- **WHEN** the viewer is in fit-to-width mode and the fit-mode command is issued
- **THEN** the viewer enters manual mode at 100% zoom (one document pixel per device pixel at standard DPI)

#### Scenario: Cycle back to fit-to-page
- **WHEN** the viewer is in manual mode at 100% zoom and the fit-mode command is issued
- **THEN** the viewer re-enters fit-to-page mode

### Requirement: Fit-to-page and fit-to-width track window resize

When fit-to-page or fit-to-width is active, the viewer SHALL recompute the zoom on every viewport resize so the page continues to satisfy the active fit rule.

#### Scenario: Fit-to-page after enlarge
- **WHEN** fit-to-page is active and the viewport grows
- **THEN** the next render uses the smaller of the width-based and height-based fit zooms for the new viewport size

#### Scenario: Fit-to-width after enlarge
- **WHEN** fit-to-width is active and the viewport grows horizontally
- **THEN** the next render uses a zoom such that the page width equals the new viewport width

### Requirement: Manual zoom with bounds

The viewer SHALL provide commands to increase and decrease the zoom by a fixed step (and a command to reset to 100%) and SHALL clamp the resulting zoom to a documented minimum and maximum so the page cannot disappear or become absurdly large.

#### Scenario: Zoom in past maximum
- **WHEN** the user issues repeated "zoom in" commands beyond the maximum zoom
- **THEN** the viewer stays at the maximum zoom and does not render beyond it

#### Scenario: Zoom out past minimum
- **WHEN** the user issues repeated "zoom out" commands below the minimum zoom
- **THEN** the viewer stays at the minimum zoom and does not render below it

#### Scenario: Reset to 100%
- **WHEN** the user issues the "100%" zoom command from any manual zoom
- **THEN** the viewer renders at 100% zoom

### Requirement: Manual zoom exits auto-fit

The viewer SHALL treat any manual zoom change (step, reset, or explicit) as exiting auto-fit so subsequent renders do not silently snap back to fit-to-page or fit-to-width.

#### Scenario: Manual zoom overrides fit
- **WHEN** the viewer is in fit-to-page or fit-to-width and the user zooms in or out or resets to 100%
- **THEN** the next render uses the manual zoom and the auto-fit flag is cleared until the fit-mode cycle is invoked again
