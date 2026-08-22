# Mouse Drag Specification

## Purpose

Lets users pan the document by clicking and dragging, matching the standard interaction model of PDF viewers like SumatraPDF.

## Requirements

### Requirement: Mouse drag panning in continuous mode

The viewer SHALL allow the user to pan the document content by holding the left mouse button and moving the mouse. The viewport SHALL scroll proportionally to the mouse movement in both horizontal and vertical directions.

#### Scenario: Vertical drag scroll
- **WHEN** the user presses the left mouse button on the document area in continuous mode and drags downward by 100 pixels
- **THEN** the viewport scrolls down by 100 pixels

#### Scenario: Horizontal drag scroll
- **WHEN** the user presses the left mouse button on the document area in continuous mode and drags leftward by 80 pixels
- **THEN** the viewport scrolls right by 80 pixels

#### Scenario: Drag boundaries
- **WHEN** the user drags beyond the document extent in any direction
- **THEN** the scroll position SHALL be clamped to valid bounds and SHALL NOT scroll past the document edges

#### Scenario: Drag ends on button release
- **WHEN** the user releases the left mouse button after dragging
- **THEN** the drag operation SHALL end and the viewport SHALL remain at the current scroll position

### Requirement: Cursor feedback during drag

The viewer SHALL change the mouse cursor to a grabbing hand while a drag operation is in progress, and restore the default cursor when the drag ends.

#### Scenario: Cursor changes on drag start
- **WHEN** the user presses the left mouse button on the document area in continuous mode
- **THEN** the mouse cursor SHALL change to a grabbing hand

#### Scenario: Cursor restores on drag end
- **WHEN** the user releases the left mouse button after dragging
- **THEN** the mouse cursor SHALL restore to the default arrow

### Requirement: Paged-mode drag pans overflowing pages

In paged mode, when the current page overflows the viewport on an axis (wider and/or taller than the viewport at the active zoom), the viewer SHALL allow dragging to move the visible part of the page on that axis, clamped to the page's overflow. Mouse clicks in paged mode on a page that fits the viewport SHALL NOT initiate a drag.

#### Scenario: Paged drag when page overflows horizontally
- **WHEN** the viewer is in paged mode and the current page is wider than the viewport, and the user drags the mouse horizontally
- **THEN** the visible part of the page moves horizontally, clamped to the page's horizontal overflow; the cursor is a grabbing hand

#### Scenario: Paged drag when page fits
- **WHEN** the viewer is in paged mode and the current page fits entirely within the viewport, and the user presses the left mouse button
- **THEN** no drag-panning is initiated and the cursor does not change

### Requirement: Settings centralisation

All UI tuning constants (scroll step sizes, panel heights, margins, buffer page counts) SHALL be defined in a single settings header file. No viewer source file SHALL hardcode these values independently.

#### Scenario: All constants in settings header
- **WHEN** a developer inspects viewer source files
- **THEN** scroll steps, panel height, page gap, margins, and buffer page count SHALL each appear in exactly one location (`viewer_settings.h`)

#### Scenario: Info panel height consistency
- **WHEN** the info panel height is referenced in code
- **THEN** the value SHALL be sourced from `viewer_settings.h`, not from local literals