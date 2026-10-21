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

### Requirement: Keyboard focus neutrality
When keyboard focus is not inside a toolbar edit box, all existing viewer keyboard shortcuts SHALL continue to work unchanged. While focus is inside an editable toolbar control, typed characters SHALL go to that control and MUST NOT trigger viewer shortcuts; after the user leaves the control, shortcuts work again. The viewer SHALL provide a way to leave an edit box from the keyboard: Escape SHALL release focus from either edit box back to the page area, and Tab and Shift+Tab SHALL traverse the toolbar's editable controls and, at either end, return focus to the page area. Committing the find box SHALL NOT move focus out of it, so repeated commits cycle matches; committing the page box SHALL return focus to the page area. Leaving an edit box SHALL NOT itself commit its contents.

#### Scenario: Typing does not trigger shortcuts
- **WHEN** the user types digits into the page box
- **THEN** no viewer shortcut actions fire while typing

#### Scenario: Shortcuts restored after leaving
- **WHEN** the user moves focus away from a toolbar edit box back to the page area
- **THEN** keyboard navigation shortcuts operate normally again

#### Scenario: Escape releases the edit box
- **WHEN** keyboard focus is inside the find box or the page box and the user presses Escape
- **THEN** focus returns to the page area and viewer shortcuts work again

#### Scenario: Tab traverses the toolbar edits
- **WHEN** keyboard focus is inside a toolbar edit box and the user presses Tab or Shift+Tab
- **THEN** focus moves to the other edit box and, pressing it again past the end, returns to the page area

#### Scenario: Repeated commits cycle the find term
- **WHEN** the find box holds a term with matches and the user presses Enter repeatedly
- **THEN** each press advances to the next match while focus stays in the find box

#### Scenario: Losing focus does not commit
- **WHEN** focus leaves a toolbar edit box without the user committing it
- **THEN** the box's contents are not committed and no search or navigation is triggered
