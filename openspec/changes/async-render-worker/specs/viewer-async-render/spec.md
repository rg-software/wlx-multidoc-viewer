## Purpose

Defines how the viewer renders pages on a background thread so that scrolling and fast navigation never block the UI, showing a placeholder for pages that are not yet rendered and repainting only the affected page when its bitmap is ready. Applies to both the Win32 and Qt viewers.

## ADDED Requirements

### Requirement: Rendering runs off the UI thread

The viewer SHALL render page bitmaps on a background worker thread rather than on the UI thread. The UI thread SHALL NOT block on a page render request.

#### Scenario: Fast scrollbar drag to the middle of a long document
- **WHEN** the user drags the scrollbar thumb quickly across a long document in continuous mode
- **THEN** the UI stays responsive throughout the drag and the viewport shows the current scroll position immediately, with placeholder pages for any that have not finished rendering

#### Scenario: Large wheel step over unrendered pages
- **WHEN** a wheel or PageDown scroll reveals pages that are not yet cached
- **THEN** the viewer paints those pages as placeholders and repaints them in place when each render completes, without a synchronous stall

### Requirement: Placeholder display for in-flight pages

The viewer SHALL display a placeholder (neutral background matching the canvas, with a brief "rendering page N" indicator) for any visible page whose bitmap is being rendered or not yet cached, instead of showing stale content.

#### Scenario: Newly visible page not yet rendered
- **WHEN** a page scrolls into view whose bitmap is not in the cache and is still being rendered
- **THEN** the page area is drawn with the placeholder until the render completes

#### Scenario: Completed render repaints in place
- **WHEN** the background render of a page finishes
- **THEN** only the affected page's region is repainted with the finished bitmap, and the placeholder is replaced

### Requirement: Render requests stay bounded to the visible window

The viewer SHALL only request renders for pages inside the visible window plus the configured cache buffer. Requests for pages that scroll out of view before start of render SHALL be dropped so that a fast drag does not queue renders for the whole document.

#### Scenario: Rapid drag to the end of the document
- **WHEN** the user drags the scrollbar thumb from the first to the last page faster than renders can complete
- **THEN** intermediate pages are never queued for rendering, and only pages near the final scroll position are rendered

#### Scenario: Cache reuse across a round trip
- **WHEN** the user scrolls away from a page and later back to it
- **THEN** the page is reused from the render cache with no re-render, provided render parameters are unchanged

### Requirement: Render parameters key the cache

Cached page bitmaps SHALL be keyed by the full render parameters (page number, zoom, rotation, DPI scale); ANY change invalidates or re-keys the cache. A render in flight with stale parameters SHALL be discarded and NOT painted.

#### Scenario: Zoom while a page is rendering
- **WHEN** the user changes zoom while some page renders are in flight
- **THEN** the in-flight results are not shown for the new zoom; the pages re-render at the new zoom

#### Scenario: Rotation invalidates cache
- **WHEN** the user rotates the document
- **THEN** all cached pages are invalidated and re-render under the new rotation