# viewer-display-modes/two-page-view

## Purpose

Defines the double-page presentation states of the viewer, in which the pages of a unit are treated as a single logical unit for paging, continuous scrolling, and fit, so book-like documents read as a facing-page spread. Two double-page states are supported: plain **double page** (pages pair consecutively from the start) and **double page with cover** (page 1 is placed in its own dedicated row, then the remaining pages pair consecutively). Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## Requirements

### Requirement: Page-presentation three-state control

The viewer SHALL support a page-presentation control with three states: single-page, double page, and double page with cover. Single-page displays a single page per view unit (the baseline behavior). Double page treats consecutive pages as one unit starting from page 1. Double page with cover places page 1 alone in its own dedicated unit and pairs the remaining pages. The presentation setting is independent of the paged/continuous mode and the active fit mode.

#### Scenario: Cycle to double page
- **WHEN** the viewer is in single-page presentation and the user activates the presentation control
- **THEN** the viewer switches to double page and re-renders, keeping the current page resolved to its unit

#### Scenario: Cycle to double page with cover
- **WHEN** the viewer is in double page presentation and the user activates the presentation control
- **THEN** the viewer switches to double page with cover, re-resolves the current page to its cover-aware unit, and re-renders

#### Scenario: Cycle back to single page
- **WHEN** the viewer is in double page with cover presentation and the user activates the presentation control
- **THEN** the viewer switches back to single page and re-renders

#### Scenario: Presentation control reflects current state
- **WHEN** the viewer is in any presentation state
- **THEN** the presentation control displays the active state's icon/label

### Requirement: Double page pairing

In double page presentation, the viewer SHALL pair pages as consecutive pairs starting from page 1: (1,2), (3,4), (5,6), : When the document has an odd page count, the final page has no partner and is shown alone.

#### Scenario: Consecutive pages pair from page 1
- **WHEN** the document has an even number of pages and the viewer is in double page presentation
- **THEN** page 1 pairs with page 2, page 3 with page 4, and so on

#### Scenario: Odd last page unpaired
- **WHEN** the document has an odd number of pages and the viewer is in double page presentation
- **THEN** the final page is shown without a partner

### Requirement: Double page with cover pairing

In double page with cover presentation, the viewer SHALL place page 1 alone in its own dedicated unit and pair the remaining pages consecutively: (1), (2,3), (4,5), (6,7), : When the number of pages remaining after page 1 is odd, the final page has no partner and is shown alone.

#### Scenario: Page 1 is a dedicated cover unit
- **WHEN** the document has 2 or more pages and the viewer is in double page with cover presentation
- **THEN** page 1 is displayed alone as its own unit

#### Scenario: Remaining pages pair after cover
- **WHEN** the viewer is in double page with cover presentation and the current page is greater than 1
- **THEN** pages are paired as (2,3), (4,5), (6,7), and so on

#### Scenario: Single-page document in cover mode
- **WHEN** the document has exactly one page and the viewer is in double page with cover presentation
- **THEN** page 1 is displayed alone (the cover unit, with no trailing pages)

### Requirement: Paged double-page display

In paged mode with a double-page presentation state, the viewer SHALL display all the pages of the current unit at once, with the current logical view unit showing the page that would have been displayed and any partner page.

#### Scenario: Full unit shows a pair
- **WHEN** the viewer is in paged mode with a double-page presentation and the current unit contains two pages
- **THEN** two adjacent pages are displayed side by side, and wheel or keyboard navigation advances to the next unit

#### Scenario: Singleton unit shown alone
- **WHEN** the viewer is in paged mode with a double-page presentation and the current unit contains one page (cover page 1 or the odd trailing page)
- **THEN** the single page is displayed alone

### Requirement: Navigation and page counter in double-page presentation

In a double-page presentation state, paged-mode next/previous commands SHALL step whole view units: from a unit they move to the adjacent unit (cover mode stepping unit by unit from page 1) and are clamped at the first and last view unit. The page counter SHALL keep showing real page numbers - the current view unit's first (leftmost) page over the physical page count - so the go-to dialog, sidebar outline, and search results keep their real-page semantics. Previous and next navigation controls SHALL be disabled exactly when no adjacent view unit exists, even though the counter's current page may still be less than the page count on the final unit.

#### Scenario: Next/prev step whole units
- **WHEN** the viewer is in paged mode with a double-page presentation and a next or previous page command is issued from a unit
- **THEN** the displayed view unit changes to the adjacent unit and the page counter shows the new unit's first page

#### Scenario: Final unit disables next
- **WHEN** the viewer is in a double-page presentation on the last view unit (for example unit (5,6) of a six-page document in double page, or the trailing singleton unit 6 in double page with cover) and the current unit's first page is still less than the page count
- **THEN** the next command is a no-op and the next navigation control is disabled, while previous remains enabled

#### Scenario: First unit disables previous
- **WHEN** the viewer is in a double-page presentation on the first view unit (page 1 alone in double page with cover)
- **THEN** the previous command is a no-op and the previous navigation control is disabled, while next remains enabled

### Requirement: Continuous double-page display

In continuous mode with a double-page presentation state, the viewer SHALL lay out the pages of each unit side by side as a single scrolling unit and scroll all pages of a unit together as if they were one page.

#### Scenario: Unit pages scroll together
- **WHEN** the viewer is in continuous mode with a double-page presentation
- **THEN** the viewport scrolls vertically through the document and all pages of the current unit scroll together as one

#### Scenario: Continuous next/prev advances one unit
- **WHEN** the viewer is in continuous mode with a double-page presentation and a keyboard or toolbar next or previous page command is issued
- **THEN** the document advances by exactly one unit and the vertical scroll position is preserved

#### Scenario: Continuous next/prev stays unit-scaled
- **WHEN** the viewer is in continuous mode with a double-page presentation and the previous or next toolbar control is invoked
- **THEN** the view advances to the neighbouring unit (the cover page is its own unit), a next step past the final unit and a previous step past the first unit are no-ops, and the controls are disabled exactly when their step cannot move

### Requirement: Fit modes in double-page presentation

In a double-page presentation state, fit-to-page SHALL target the whole current unit rather than a single page: the current unit fits inside the viewport, and manual zoom continues to apply to the whole unit. Fit-to-width SHALL target the two-page spread rather than a single page: the zoom is computed so the width of the widest combined unit in the document equals the viewport width, so a singleton unit (the cover page, or a trailing odd page) does not collapse the fit onto one page and the spread that follows is guaranteed to fit horizontally without overflow.

#### Scenario: Fit unit to viewport
- **WHEN** the viewer is in a double-page presentation and fit-to-page is active
- **THEN** the zoom is computed so the current unit fits inside the viewport

#### Scenario: Fit spread width to viewport
- **WHEN** the viewer is in a double-page presentation and fit-to-width is active
- **THEN** the zoom is computed so the width of the document's widest two-page unit equals the viewport width

#### Scenario: Fit-to-width away from a spread
- **WHEN** the viewer is in a double-page presentation, fit-to-width is active, and the current unit is a singleton (the cover page or a trailing odd page)
- **THEN** the zoom still uses the two-page spread width as the target, so the singleton renders at a fraction of the viewport width and the next spread fits without overflow
