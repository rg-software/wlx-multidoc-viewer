# Viewer Scrolling Specification

## Purpose

Defines how continuous mode lays out pages, computes scroll ranges, and renders visible pages so that scrollbar navigation is accurate across the full document, scrolling stays responsive, and differently-sized pages stack correctly without shifting as the viewer navigates. Applies to both the Win32 and Qt viewers.

## Requirements

### Requirement: Per-page continuous layout with real geometry

The viewer SHALL lay out continuous-mode pages with each page's own scaled dimensions (page size * zoom * DPI scale, after rotation), stacked vertically with a fixed gap, and horizontally centered in the content area. The viewer MUST NOT use a uniform stride or the current page's size as a proxy for every page.

#### Scenario: Mixed page sizes in continuous mode
- **WHEN** a document contains pages of different aspect ratios (e.g. portrait and landscape) and continuous mode is active
- **THEN** each page is positioned according to its own scaled height and width, with the gap between pages constant and each page horizontally centered

#### Scenario: Scroll offset maps to the correct page
- **WHEN** the viewer is scrolled to a vertical offset that is inside the page-range of page N (per the per-page layout)
- **THEN** the viewer reports N as the current page

#### Scenario: Document end reached
- **WHEN** the viewer is scrolled to the bottom of the virtual canvas
- **THEN** the last page is reported as the current page

### Requirement: Scrollbar range covers the full document height

The scrollbar range SHALL be computed from the total height of the per-page layout (sum of all page heights plus gaps), not from a capped bitmap height. Dragging the scrollbar thumb to any position SHALL select the proportional vertical offset in the full document.

#### Scenario: Fast navigation to the middle of a long document
- **WHEN** the user drags the scrollbar thumb to the approximate middle of a 200-page document
- **THEN** the viewer displays a page near the middle of that document (e.g. within a few pages of page 100), not an earlier page

#### Scenario: Long document at high zoom
- **WHEN** a document is long enough that its total layout height exceeds the previous fixed strip cap
- **THEN** the scrollbar still allows reaching every page of the document (the cap is no longer applied to the scroll range)

### Requirement: Render only pages near the viewport

The viewer SHALL render only pages that intersect the viewport plus a small buffer above and below, reusing a per-page render cache so that scrolling over already-visited pages performs no re-rendering. Pages outside the rendered window SHALL appear as a neutral background until scrolled into view.

#### Scenario: ScrollDelta move smaller than the viewport
- **WHEN** the user scrolls by a small delta (e.g. one wheel notch)
- **THEN** no heavy re-composition of the full document is performed; pages already cached are reused and any newly-revealed page from the buffer is rendered

#### Scenario: Returning to a previously visited page
- **WHEN** the user scrolls back to a page that was rendered earlier in the same viewing session
- **THEN** the page is shown from the cache without re-rendering

### Requirement: Zoom and layout changes preserve the viewport anchor

When the zoom, rotation, or fit mode changes, the viewer SHALL recompute the per-page layout but anchor the viewport so the page that was at the top of the viewport (or the corresponding fraction of it) stays at the top. The viewer SHALL NOT recompute fit zoom against the currently selected page in a way that shifts the whole layout.

#### Scenario: Zoom in on a page in continuous mode
- **WHEN** the user zooms in on a page while in continuous mode
- **THEN** the pages that were near the top of the viewport remain near the top after the re-layout, with no unexpected jump to a different part of the document

#### Scenario: Rotate a document with mixed page sizes
- **WHEN** the user applies a 90° rotation to a mixed-size document in continuous mode
- **THEN** the layout is recomputed from the rotated per-page geometries and the viewport stays anchored to the same region of the document

### Requirement: Virtual canvas memory bound

The viewer SHALL bound the memory used for rendered pages in continuous mode to the per-page cache, not to a single tall strip bitmap. The total height of the virtual canvas SHALL be represented without allocating a bitmap of that height.

#### Scenario: Very long document in continuous mode
- **WHEN** the user opens a very long document (e.g. more pages than fit in a fixed-height strip) and enables continuous mode
- **THEN** the viewer does not allocate a single bitmap taller than the per-page cache budget, yet the scrollbar reaches the end of the document