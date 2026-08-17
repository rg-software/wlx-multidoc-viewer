## Purpose

Ensures pages render sharply and without distortion at any window size, with proper DPI awareness and zoom calculations that keep content readable.

## ADDED Requirements

### Requirement: Distortion-free resize
The viewer SHALL render the page without stretching, squashing, or aspect ratio distortion when the window is resized.

#### Scenario: Window wider than page
- **WHEN** the viewer window is wider than the rendered page at current zoom
- **THEN** the page is displayed at its natural aspect ratio, centered horizontally

#### Scenario: Window narrower than page
- **WHEN** the viewer window is narrower than the rendered page at current zoom
- **THEN** the page is displayed at its natural aspect ratio with horizontal scroll enabled

### Requirement: Fit-to-width zoom
The viewer SHALL support a fit-to-width mode where the page width matches the viewport width.

#### Scenario: Fit-to-width on resize
- **WHEN** fit-to-width is enabled and the window is resized
- **THEN** the page zoom is recalculated so the page width fills the viewport width

#### Scenario: Fit-to-width preserves aspect ratio
- **WHEN** fit-to-width is active
- **THEN** the page height scales proportionally to the width

### Requirement: DPI-aware rendering
The viewer SHALL render text and vector content at the native display DPI, not at 96 DPI.

#### Scenario: High-DPI display
- **WHEN** the display is set to 150% scaling (144 DPI)
- **THEN** text appears sharp and correctly sized relative to the OS UI

### Requirement: Sharp text at zoom levels
The viewer SHALL render text without blurriness at common zoom levels (50%, 100%, 150%, 200%).

#### Scenario: 100% zoom
- **WHEN** the zoom level is 100%
- **THEN** one document pixel maps to one screen pixel (adjusted for DPI)

#### Scenario: 200% zoom
- **WHEN** the zoom level is 200%
- **THEN** each document pixel maps to a 2x2 block of screen pixels
