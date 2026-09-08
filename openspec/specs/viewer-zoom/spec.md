# Viewer Zoom Specification

## Purpose

Defines how the viewer scales the displayed page, including the fit-mode cycle (fit-to-page → fit-to-width → 100%) and manual zoom step. Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## Requirements

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