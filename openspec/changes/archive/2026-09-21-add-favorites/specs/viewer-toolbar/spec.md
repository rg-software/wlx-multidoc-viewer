## ADDED Requirements

### Requirement: Favorites toggle control
The toolbar SHALL provide a bookmark-style button that toggles the current page's favorite status, behaving identically to the viewer's keyboard shortcut and to the action described by the `favorites` capability. The button SHALL reflect the current page's favorite status (pressed/checked when the current page is a favorite, released otherwise) and SHALL be placed near the zoom controls. The button SHALL be present on both platforms.

#### Scenario: Add a favorite from the toolbar
- **WHEN** the current page is not a favorite and the user clicks the favorites button on either platform
- **THEN** the page becomes a favorite and the button shows the pressed state

#### Scenario: Remove a favorite from the toolbar
- **WHEN** the current page is a favorite and the user clicks the favorites button on either platform
- **THEN** the page is no longer a favorite and the button shows the released state

#### Scenario: Button state follows navigation
- **WHEN** the user navigates to a page
- **THEN** the favorites button shows the pressed state if that page is a favorite and the released state otherwise

## MODIFIED Requirements

### Requirement: Outline sidebar availability
The viewer SHALL provide a toggleable sidebar docked at the left edge of the window listing the open document's embedded outline (table of contents) whenever the document exposes one. Entries SHALL reflect the outline's hierarchy, and activating an entry SHALL navigate to its target page. Whenever the open document has favorites, the sidebar SHALL also show a dedicated, flat Favorites section directly below the outline entries, as specified by the `favorites` capability. The viewer SHALL make the sidebar available whenever the open document exposes an outline OR has favorites; when it has neither, the viewer MUST NOT show a sidebar and its toggle control MUST be unavailable. The sidebar's visibility SHALL persist across resizes without overlapping the page area.

#### Scenario: Toggle reveals outline
- **WHEN** a document with an embedded outline is open and the user activates the sidebar toggle
- **THEN** a left sidebar appears showing the hierarchical table of contents without shrinking the toolbar or info panel

#### Scenario: Entry navigation
- **WHEN** the user activates an outline entry
- **THEN** the viewer navigates to that entry's target page

#### Scenario: Document without outline
- **WHEN** the viewer opens a document whose engine provides no outline and that has no favorites
- **THEN** no sidebar exists and the toggle control is hidden or disabled

#### Scenario: Favorites alone make the sidebar available
- **WHEN** the viewer opens a document whose engine provides no outline but that has favorites
- **THEN** the sidebar toggle is available and, when the sidebar is shown, it displays the Favorites section