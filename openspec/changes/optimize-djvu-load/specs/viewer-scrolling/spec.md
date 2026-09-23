## MODIFIED Requirements

### Requirement: Per-page continuous layout with real geometry

The viewer SHALL lay out continuous-mode pages with each page's own scaled dimensions (page size * zoom * DPI scale, after rotation), stacked vertically with a fixed gap, and horizontally centered in the content area. The viewer MAY build the layout initially from a provisional page geometry (for example, the size of the first measured page) and then replace each page's provisional geometry with its measured size as measurement completes. The viewer MUST NOT permanently use a uniform stride or a single page's size as a proxy: once a page's measurement has completed, its layout entry MUST reflect that page's own dimensions.

#### Scenario: Mixed page sizes in continuous mode
- **WHEN** a document contains pages of different aspect ratios (e.g. portrait and landscape) and continuous mode is active
- **THEN** each page is positioned according to its own scaled height and width, with the gap between pages constant and each page horizontally centered

#### Scenario: Scroll offset maps to the correct page
- **WHEN** the viewer is scrolled to a vertical offset that is inside the page-range of page N (per the per-page layout)
- **THEN** the viewer reports N as the current page

#### Scenario: Document end reached
- **WHEN** the viewer is scrolled to the bottom of the virtual canvas
- **THEN** the last page is reported as the current page

#### Scenario: Provisional geometry replaced by measured geometry
- **WHEN** the layout was built from a provisional page size and a page's measured dimensions become available
- **THEN** that page's layout entry is updated to its measured scaled dimensions

### Requirement: Scrollbar range covers the full document height

The scrollbar range SHALL be computed from the total height of the per-page layout (sum of all page heights plus gaps), not from a capped bitmap height. Dragging the scrollbar thumb to any position SHALL select the proportional vertical offset in the full document. While some page geometries are still provisional the range MAY be based on the provisional total, but it MUST converge to the measured total once measurement completes and MUST always allow reaching every page of the document.

#### Scenario: Fast navigation to the middle of a long document
- **WHEN** the user drags the scrollbar thumb to the approximate middle of a 200-page document
- **THEN** the viewer displays a page near the middle of that document (e.g. within a few pages of page 100), not an earlier page

#### Scenario: Long document at high zoom
- **WHEN** a document is long enough that its total layout height exceeds the previous fixed strip cap
- **THEN** the scrollbar still allows reaching every page of the document (the cap is no longer applied to the scroll range)

#### Scenario: Range converges after measurement
- **WHEN** the scrollbar range was based on provisional page geometries and all page measurements complete
- **THEN** the range reflects the measured total page height and every page remains reachable

## ADDED Requirements

### Requirement: Anchored geometry refinement

When a page's measured geometry replaces its provisional geometry, the viewer SHALL keep the document region under the viewport anchored (the page at the top of the viewport and its fractional offset stay in view) and SHALL preserve already-rendered pages and link hot zones for pages whose geometry did not change. Refinement MUST NOT jump the viewport to a different part of the document and MUST NOT discard cached renders of unchanged pages.

#### Scenario: Refinement while reading a page
- **WHEN** the user is reading a page whose geometry is provisional and the viewer refines geometry for other pages
- **THEN** the page under the viewport stays in place and its rendered bitmap is not discarded

#### Scenario: Refinement of the page under the viewport
- **WHEN** the measured geometry of the page currently under the viewport replaces its provisional geometry
- **THEN** the viewport stays anchored to the same fractional position within that page
