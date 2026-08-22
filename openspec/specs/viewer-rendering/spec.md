# Viewer Rendering Specification

## Purpose

Defines how the viewer renders document pages onto the screen, including the render pipeline, DPI awareness, aspect-ratio preservation, resize behavior, and page rotation. Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## Requirements

### Requirement: Render current page on demand

The viewer SHALL render the current page into a bitmap sized by the requested zoom factor and the active display DPI scale, and display that bitmap in the viewport.

#### Scenario: Initial render after load
- **WHEN** a document is successfully loaded and the current page is set
- **THEN** the viewer displays the bitmap for the current page within the viewport

#### Scenario: Re-render on page change
- **WHEN** the current page index changes
- **THEN** the viewer replaces the displayed bitmap with a freshly rendered one for the new page

### Requirement: Render at native display DPI

The viewer SHALL request that engines render at the host system's effective DPI scale rather than always at 96 DPI.

#### Scenario: High DPI display
- **WHEN** the host display is configured at 150% scaling (144 DPI effective)
- **THEN** rendered text and vector content appear sharp at the OS UI scale, not undersized

#### Scenario: Standard DPI display
- **WHEN** the host display is at 100% scaling (96 DPI effective)
- **THEN** the render is equivalent to a 1:1 device-pixel mapping and matches the OS UI scale

### Requirement: Preserve natural aspect ratio

The viewer SHALL display the page at its natural aspect ratio. The viewer MUST NOT stretch or squash the bitmap to fit the viewport.

#### Scenario: Wider viewport than page
- **WHEN** the viewport is wider than the rendered page at the active zoom
- **THEN** the page is displayed at its natural width, centered, with the remaining horizontal space unused

#### Scenario: Narrower viewport than page
- **WHEN** the viewport is narrower than the rendered page at the active zoom
- **THEN** the page is displayed at its natural width and the viewport offers horizontal scrolling

### Requirement: Reflow on resize without distortion

The viewer SHALL adjust the displayed bitmap when the viewport is resized so that aspect ratio is preserved and no stretching, squashing, or partial duplication occurs.

#### Scenario: Window resized wider
- **WHEN** the viewport width increases
- **THEN** the next render keeps the page's natural aspect ratio (recompute zoom under auto-fit, or scroll under manual zoom)

#### Scenario: Window resized narrower
- **WHEN** the viewport width decreases
- **THEN** the next render keeps the page's natural aspect ratio and may enable horizontal or vertical scroll

### Requirement: Pixel-aligned render at integer zoom levels

The viewer SHALL render such that at integer zoom factors (50%, 100%, 150%, 200%) combined with the DPI scale, the resulting bitmap aligns to whole device pixels so text is not blurry.

#### Scenario: 100% zoom at standard DPI
- **WHEN** the zoom level is 100% and DPI scale is 1.0
- **THEN** one document pixel maps to one device pixel

#### Scenario: 200% zoom at 150% DPI
- **WHEN** the zoom level is 200% and DPI scale is 1.5
- **THEN** the render is at 3x device pixels per document pixel with no fractional scaling

### Requirement: Rotate pages by 90° steps

The viewer SHALL support rotating the rendered page by 90° clockwise and 90° counter-clockwise. The rotation is applied as a render transform that affects every page of the open document and persists for the lifetime of that document.

#### Scenario: Rotate clockwise
- **WHEN** the viewer receives a rotate-cw command
- **THEN** the page is rendered rotated 90° clockwise relative to its natural orientation and the bitmap dimensions swap accordingly (width ↔ height)

#### Scenario: Rotate counter-clockwise
- **WHEN** the viewer receives a rotate-ccw command
- **THEN** the page is rendered rotated 90° counter-clockwise relative to its natural orientation

#### Scenario: Rotation persists across page navigation
- **WHEN** the user rotates the document and then navigates to another page
- **THEN** the newly rendered page is rotated by the same angle

#### Scenario: Rotation resets on new document
- **WHEN** a new document is loaded
- **THEN** the rotation angle is reset to 0°

#### Scenario: Rotation respects zoom and DPI
- **WHEN** the document is rotated and zoom or DPI is changed
- **THEN** the rotated page is rendered at the new zoom and DPI without re-evaluating the rotation

### Requirement: Continuous-mode strip renders visible pages and centers each page horizontally

When continuous mode is active, the viewer SHALL render pages of the document stacked vertically with a small gap between consecutive pages. Pages within and near the viewport (3 pages buffer above and below) SHALL be rendered; pages outside this range remain as background until scrolled into view. Each page SHALL be centered horizontally within the strip (i.e. the left margin equals the right margin within the strip row). The strip width SHALL be the wider of the rendered page width and the viewport width. The strip height SHALL represent the full document height for correct scrollbar range, regardless of which pages are currently rendered.

#### Scenario: Pages centered in strip
- **WHEN** continuous mode is active and the rendered page width is smaller than the viewport width
- **THEN** each page is centered horizontally within its row in the strip, with equal unused margins on both sides