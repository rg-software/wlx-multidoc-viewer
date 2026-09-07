## Purpose

Lets the user flip through all raster images in a folder from within the lister — next/prev at a standalone image document's boundary opens the adjacent sibling image in natural order instead of clamping.

## ADDED Requirements

### Requirement: Detect sibling images at load

When the viewer opens a standalone raster image, it SHALL find the other raster images in the same directory so the image can be navigated as part of its neighborhood.

#### Scenario: Siblings discovered on open
- **WHEN** the user opens a raster image in a directory containing other raster images
- **THEN** the viewer records those sibling images for boundary navigation

#### Scenario: No siblings yields empty neighborhood
- **WHEN** the user opens a raster image in a directory with no other raster images
- **THEN** the viewer records an empty sibling set and boundary navigation is unavailable

### Requirement: Boundary-spanning next/prev

For a standalone image document, issuing next at the final page or prev at the first page SHALL open the adjacent sibling image when one exists, instead of staying put.

#### Scenario: Next at last page opens next sibling
- **WHEN** the user is on the sole page of an image document and a sibling exists after it in natural order, and a "next" command is issued
- **THEN** the viewer opens that sibling image in place

#### Scenario: Prev at first page opens previous sibling
- **WHEN** the user is on the sole page of an image document and a sibling exists before it in natural order, and a "previous" command is issued
- **THEN** the viewer opens that sibling image in place

#### Scenario: Boundary without sibling clamps
- **WHEN** the user is at a document boundary and no sibling exists on that side in natural order
- **THEN** the viewer remains on the current page (existing clamp behavior applies)

### Requirement: Deterministic natural ordering

The viewer SHALL order sibling images using natural (numeric-aware) comparison of their filenames, shared with comic archives, so that adjacent siblings are deterministic.

#### Scenario: Numeric sibling order
- **WHEN** a directory contains `img2.png` and `img10.png` on either side of the open image
- **THEN** `img2` precedes `img10` in sibling order regardless of lexicographic position