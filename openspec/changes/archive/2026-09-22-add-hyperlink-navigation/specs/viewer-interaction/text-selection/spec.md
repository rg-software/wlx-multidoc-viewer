## MODIFIED Requirements

### Requirement: Cursor feedback over selectable text

The viewer SHALL show the standard I-beam cursor while the pointer hovers over selectable text within a page, the pointing-hand cursor while the pointer is over a hyperlink hot zone with the link-activation modifier (Ctrl) held (see `viewer-hyperlinks`), and the usual arrow cursor everywhere else.

#### Scenario: Hovering over a word
- **WHEN** the pointer moves over glyphs of a selectable page that are not part of a hyperlink
- **THEN** the cursor becomes I-beam

#### Scenario: Hovering over a hyperlink with the modifier held
- **WHEN** the pointer moves over a hyperlink hot zone, even where selectable text underlies it, and the activation modifier is held
- **THEN** the cursor becomes the pointing hand rather than the I-beam

#### Scenario: Hovering over a hyperlink without the modifier
- **WHEN** the pointer moves over a hyperlink hot zone without the activation modifier held
- **THEN** the cursor follows the text rule (I-beam when selectable text underlies it)

#### Scenario: Hovering outside text
- **WHEN** the pointer sits over a page margin, background, or a non-selectable page, and not over a hyperlink with the modifier held
- **THEN** the cursor returns to the default viewer cursor

### Requirement: Selection versus drag-pan precedence

A left-button press that does not begin on selectable text SHALL preserve the existing drag-pan behavior; only presses starting on selectable text begin a selection. A Ctrl+click that begins on a hyperlink hot zone SHALL instead follow that link (see `viewer-hyperlinks`) and SHALL NOT start a selection; the same press without Ctrl SHALL still start a selection when it begins on selectable text.

#### Scenario: Press on empty margin
- **WHEN** the user left-drags starting on page background or outside any page
- **THEN** the view pans as before and no selection is started

#### Scenario: Ctrl+click on a hyperlink
- **WHEN** the user Ctrl+clicks a hyperlink hot zone that overlies selectable text
- **THEN** the link is followed and no text selection is started

#### Scenario: Plain click on a hyperlink over text
- **WHEN** the user left-clicks the same hyperlink hot zone without holding Ctrl
- **THEN** a text selection starts as before and no link is followed
