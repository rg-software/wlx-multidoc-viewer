## MODIFIED Requirements

### Requirement: Render a CHM page

The viewer SHALL render the selected CHM page to a bitmap suitable for the lister's paint path, sized by the requested zoom and DPI.

#### Scenario: Render a page
- **WHEN** the user is on a CHM page and the viewer paints
- **THEN** the viewer renders that page's HTML content to the bitmap, honoring zoom and DPI

#### Scenario: HTML with relative links
- **WHEN** the CHM page contains relative links to other HTML entries in the same archive
- **THEN** each such link is exposed as a navigable link hot zone whose destination is resolved against the current topic's archive path

#### Scenario: Link to another topic
- **WHEN** the user activates a relative link to another HTML entry in the archive
- **THEN** the viewer jumps to that entry's page in the CHM page list

#### Scenario: Link with a fragment
- **WHEN** the user activates a CHM link carrying a `#fragment` that names an anchor in the target topic
- **THEN** the viewer shows the target topic's page scrolled to that anchor when it resolves, or from the top otherwise

#### Scenario: Link outside the archive
- **WHEN** the CHM page contains a link to an external URI rather than an archive entry
- **THEN** the link is handed to the system handler as defined by `viewer-hyperlinks`
