## MODIFIED Requirements

### Requirement: Outline sidebar availability

The viewer SHALL provide a toggleable sidebar docked at the left edge of the window listing the open document's embedded outline (table of contents) whenever the document exposes one. Entries SHALL reflect the outline's hierarchy, and activating an entry SHALL navigate to its target page. The engine SHALL resolve every outline entry to a destination the viewer can navigate to, including entries whose destination the underlying document library leaves unresolved and only exposes as a target reference (for example an EPUB table of contents, whose entries reference a content document and an optional fragment). Such an entry SHALL resolve to the page that holds the referenced content and, when the reference names a fragment, SHALL carry the position of that fragment inside the page so that activating it lands on the referenced content rather than at the top of the page. An entry whose destination cannot be resolved SHALL remain listed but SHALL be marked as having no destination, so that it is not treated as a page reference when the viewer tracks the reading position; activating such an entry SHALL fall back to the first page. Whenever the open document has favorites, the sidebar SHALL also show a dedicated, flat Favorites section directly below the outline entries, as specified by the `favorites` capability. The viewer SHALL make the sidebar available whenever the open document exposes an outline OR has favorites; when it has neither, the viewer MUST NOT show a sidebar and its toggle control MUST be unavailable. The sidebar's visibility SHALL persist across resizes without overlapping the page area.

#### Scenario: Toggle reveals outline
- **WHEN** a document with an embedded outline is open and the user activates the sidebar toggle
- **THEN** a left sidebar appears showing the hierarchical table of contents without shrinking the toolbar or info panel

#### Scenario: Entry navigation
- **WHEN** the user activates an outline entry
- **THEN** the viewer navigates to that entry's target page

#### Scenario: Table of contents whose destinations the library leaves unresolved
- **WHEN** the viewer opens a document whose table of contents references its content by document reference rather than by page, such as an EPUB
- **THEN** every entry resolves to the page that holds the referenced content, and activating the entry navigates to that page

#### Scenario: Table of contents entry naming a fragment
- **WHEN** an outline entry references a fragment inside a content document
- **THEN** activating the entry navigates to the page that holds the fragment and positions the view on the fragment rather than at the top of the page

#### Scenario: Table of contents entry with an unresolvable destination
- **WHEN** an outline entry references a target that the document does not contain
- **THEN** the entry stays listed, is not treated as a page reference when the viewer tracks the reading position, and activating it falls back to the first page

#### Scenario: Document without outline
- **WHEN** the viewer opens a document whose engine provides no outline and that has no favorites
- **THEN** no sidebar exists and the toggle control is hidden or disabled

#### Scenario: Favorites alone make the sidebar available
- **WHEN** the viewer opens a document whose engine provides no outline but that has favorites
- **THEN** the sidebar toggle is available and, when the sidebar is shown, it displays the Favorites section
