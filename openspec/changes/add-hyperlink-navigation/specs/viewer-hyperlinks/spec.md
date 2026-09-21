## Purpose

Lets users follow the hyperlinks embedded in rendered documents — internal cross-references and external URLs — on both the Win32 and Qt viewers, so link-bearing PDF, EPUB, XPS, MOBI, and CHM content is navigable instead of inert.

## ADDED Requirements

### Requirement: Expose per-page link regions

The viewer SHALL determine, for each page that carries hyperlinks, the set of link hot zones as rectangles in unrotated page space (y-down, pre-zoom, relative to the engine's page dimensions), each associated with either an internal destination or an external URI. Documents and pages without hyperlinks SHALL behave exactly as before this capability.

#### Scenario: Page with links
- **WHEN** a PDF, EPUB, XPS, MOBI, or CHM page containing hyperlinks is displayed
- **THEN** the viewer knows that page's link hot zones and their destinations

#### Scenario: Page without links
- **WHEN** a page with no hyperlinks is displayed
- **THEN** no link affordances appear and pointer behavior is unchanged

#### Scenario: Link extent follows zoom and rotation
- **WHEN** a page bearing links is zoomed, rotated, or shown in continuous mode
- **THEN** each link's hot zone remains anchored to the same rendered link text

### Requirement: Pointing-hand cursor over links

The viewer SHALL show the system pointing-hand cursor while the pointer is over a link hot zone, and SHALL restore the previously applicable cursor (for example the I-beam over selectable text, or the arrow/hand otherwise) once the pointer leaves all link hot zones.

#### Scenario: Hover a link that overlies text
- **WHEN** the pointer moves over a link whose hot zone also contains selectable text
- **THEN** the cursor is the pointing hand, not the I-beam

#### Scenario: Leave a link
- **WHEN** the pointer moves off a link hot zone onto ordinary selectable text
- **THEN** the cursor returns to the I-beam

### Requirement: Activate links with Ctrl+click

The viewer SHALL follow a link only when the user presses the primary mouse button while the platform link-activation modifier (Ctrl) is held down. A primary-button press without the modifier SHALL NOT follow links and SHALL retain the existing text-selection and drag behavior. Both platforms SHALL use the same modifier and the same behavior.

#### Scenario: Ctrl+click on a link
- **WHEN** the user holds Ctrl and clicks a link hot zone
- **THEN** the link is followed

#### Scenario: Plain click on a link over text
- **WHEN** the user clicks a link hot zone that overlies selectable text without holding Ctrl
- **THEN** no link is followed and text selection or drag proceeds as before

### Requirement: Follow internal link destinations

For an internal link the viewer SHALL navigate to the link's destination page and, when the link carries an in-page anchor, position the destination so that anchor is at the top of the page area. Destination pages SHALL be resolved in the viewer's public page numbering, honoring any engine-level page offset (such as a synthetic cover page). Link navigation SHALL use the same re-fit and scroll-anchoring behavior as other page navigation. A destination that cannot be resolved SHALL leave the current view unchanged.

#### Scenario: Link to another page
- **WHEN** the user activates an internal link targeting a different page
- **THEN** the viewer displays the destination page

#### Scenario: Link to an in-page anchor
- **WHEN** the user activates a link whose destination carries a fragment or positional anchor on the target page
- **THEN** the viewer shows the target page scrolled so the anchor is at the top of the page area

#### Scenario: Anchor cannot be resolved
- **WHEN** the user activates a link whose anchor is missing
- **THEN** the viewer shows the destination page from its top

#### Scenario: Unresolvable destination
- **WHEN** the user activates an internal link whose destination does not resolve to any page
- **THEN** the current view is unchanged and the viewer does not crash

### Requirement: Open external links with the system handler

For an external link the viewer SHALL hand the URI to the operating system's default handler and SHALL NOT attempt to render the target itself. A failed or unavailable handler SHALL leave the displayed document unchanged. Each platform SHALL use its native launcher.

#### Scenario: External web link
- **WHEN** the user activates an `http` or `https` link
- **THEN** the system's default browser opens the URI and the displayed document is unchanged

#### Scenario: Non-web external scheme
- **WHEN** the user activates a `mailto` or other external-scheme link
- **THEN** the URI is passed to the system handler for that scheme

#### Scenario: Launch failure
- **WHEN** the operating system cannot open the URI
- **THEN** the viewer shows no error dialog and the document remains displayed
