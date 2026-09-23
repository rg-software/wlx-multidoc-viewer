## MODIFIED Requirements

### Requirement: Text-find controls
The toolbar SHALL provide a find text box, previous-match and next-match buttons, and a match-case toggle, presenting the text-search capability described in `viewer-text-search`. The find box and match-case toggle SHALL be enabled whenever a searchable document is open. The previous-match and next-match buttons SHALL be enabled whenever a searchable document is open and the find box is non-empty, independent of whether a result set currently exists.

#### Scenario: Find controls present
- **WHEN** a searchable document is open
- **THEN** the toolbar shows the find box, match navigation buttons, and match-case toggle

#### Scenario: Match buttons follow box content
- **WHEN** a searchable document is open and the user types the first character into the find box
- **THEN** the previous-match and next-match buttons become enabled before any search is committed

#### Scenario: Match buttons disabled on empty box
- **WHEN** the find box is empty
- **THEN** the previous-match and next-match buttons are disabled

### Requirement: Toolbar state synchronization
The toolbar SHALL reflect viewer state changes regardless of their origin: navigating by keyboard or mouse updates the page box, zoom changes update enabled state of zoom buttons, and mode or fit changes update the corresponding button visuals. A change to the find box contents SHALL update find-control enablement immediately, without requiring a commit. Toolbar-originated actions and external state changes MUST NOT diverge.

#### Scenario: Keyboard navigation reflected
- **WHEN** the user moves to another page with keyboard shortcuts
- **THEN** the page box shows the new current page

#### Scenario: Find box edit reflected
- **WHEN** the user types into or clears the find box
- **THEN** find-control enablement updates on that edit, before any search is committed
