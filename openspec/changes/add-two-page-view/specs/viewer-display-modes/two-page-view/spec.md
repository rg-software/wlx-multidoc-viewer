## Purpose

Defines the double-page presentation states of the viewer, in which the pages of a unit are treated as a single logical unit for paging, continuous scrolling, and fit, so book-like documents read as a facing-page spread. Two double-page states are supported: plain **double page** (pages pair consecutively from the start) and **double page with cover** (page 1 is placed in its own dedicated row, then the remaining pages pair consecutively). Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## ADDED Requirements

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

In double page presentation, the viewer SHALL pair pages as consecutive pairs starting from page 1: (1,2), (3,4), (5,6), … When the document has an odd page count, the final page has no partner and is shown alone.

#### Scenario: Consecutive pages pair from page 1
- **WHEN** the document has an even number of pages and the viewer is in double page presentation
- **THEN** page 1 pairs with page 2, page 3 with page 4, and so on

#### Scenario: Odd last page unpaired
- **WHEN** the document has an odd number of pages and the viewer is in double page presentation
- **THEN** the final page is shown without a partner

### Requirement: Double page with cover pairing

In double page with cover presentation, the viewer SHALL place page 1 alone in its own dedicated unit and pair the remaining pages consecutively: (1), (2,3), (4,5), (6,7), … When the number of pages remaining after page 1 is odd, the final page has no partner and is shown alone.

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

### Requirement: Continuous double-page display

In continuous mode with a double-page presentation state, the viewer SHALL lay out the pages of each unit side by side as a single scrolling unit and scroll all pages of a unit together as if they were one page.

#### Scenario: Unit pages scroll together
- **WHEN** the viewer is in continuous mode with a double-page presentation
- **THEN** the viewport scrolls vertically through the document and all pages of the current unit scroll together as one

#### Scenario: Continuous next/prev advances one unit
- **WHEN** the viewer is in continuous mode with a double-page presentation and a keyboard next or previous page command is issued
- **THEN** the document advances by exactly one unit and the vertical scroll position is preserved

### Requirement: Fit modes in double-page presentation

In a double-page presentation state, the fit modes SHALL target the whole current unit rather than a single page: fit-to-page SHALL fit the unit inside the viewport, fit-to-width SHALL make the combined width of the unit equal the viewport width, and manual zoom continues to apply to the whole unit.

#### Scenario: Fit unit to viewport
- **WHEN** the viewer is in a double-page presentation and fit-to-page is active
- **THEN** the zoom is computed so the current unit fits inside the viewport

#### Scenario: Fit unit width to viewport
- **WHEN** the viewer is in a double-page presentation and fit-to-width is active
- **THEN** the zoom is computed so the combined width of the current unit equals the viewport width
