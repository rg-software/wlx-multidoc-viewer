## MODIFIED Requirements

### Requirement: Follow internal link destinations

For an internal link the viewer SHALL navigate to the link's destination page and, when the link carries an in-page anchor, position the destination so that anchor is at the top of the page area. Destination pages SHALL be resolved in the viewer's public page numbering, honoring any engine-level page offset (such as a synthetic cover page). Link navigation SHALL use the same re-fit and scroll-anchoring behavior as other page navigation. A destination that cannot be resolved SHALL leave the current view unchanged. The same destination SHALL resolve identically regardless of how it is reached: an in-page link, an outline entry in the sidebar, or any other reference to the same target SHALL lead to the same page and the same in-page position.

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

#### Scenario: Same target reached through the outline
- **WHEN** the document exposes an outline entry that references the same target as an in-page link
- **THEN** activating the outline entry leads to the same page and the same in-page position as activating the link
