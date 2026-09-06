## Purpose

Defines the two-page presentation of the viewer, in which two adjacent pages are treated as a single logical unit for paging, continuous scrolling, and fit, so book-like documents read as a facing-page spread. Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## ADDED Requirements

### Requirement: Two-page presentation toggle

The viewer SHALL support a presentation toggle between one-page and two-page viewing. One-page presentation displays a single page per view unit; two-page presentation treats two adjacent pages as one unit. The presentation setting is independent of the paged/continuous mode and the active fit mode.

#### Scenario: Toggle to two-page
- **WHEN** the viewer is in one-page presentation and the user activates the presentation toggle
- **THEN** the viewer switches to two-page presentation and re-renders, keeping the current page index

#### Scenario: Toggle back to one-page
- **WHEN** the viewer is in two-page presentation and the user activates the presentation toggle
- **THEN** the viewer switches back to one-page presentation and re-renders

### Requirement: Paged two-page display

In paged mode with two-page presentation, the viewer SHALL display two adjacent pages at once, with the current logical view unit showing the page that would have been displayed and its partner page.

#### Scenario: Even view unit shows a pair
- **WHEN** the viewer is in paged mode with two-page presentation and the active page is paired
- **THEN** two adjacent pages are displayed side by side, and wheel or keyboard navigation advances to the next pair of pages

#### Scenario: Odd trailing page shown alone
- **WHEN** the viewer is in paged mode with two-page presentation and the active page is the last remaining page with no partner
- **THEN** the final page is displayed alone

### Requirement: Continuous two-page display

In continuous mode with two-page presentation, the viewer SHALL lay out two adjacent pages side by side as a single scrolling unit and scroll both pages together as if they were one page.

#### Scenario: Two pages scroll together
- **WHEN** the viewer is in continuous mode with two-page presentation
- **THEN** the viewport scrolls vertically through the document and both pages of the current unit scroll together as one

#### Scenario: Continuous next/prev advances both
- **WHEN** the viewer is in continuous mode with two-page presentation and a keyboard next or previous page command is issued
- **THEN** the document advances by one two-page unit and the vertical scroll position is preserved

### Requirement: Fit modes in two-page presentation

In two-page presentation, the fit modes SHALL target the whole two-page unit rather than a single page: fit-to-page SHALL fit both pages together inside the viewport, fit-to-width SHALL make the combined width of both pages equal the viewport width, and manual zoom continues to apply to the whole unit.

#### Scenario: Fit both pages to viewport
- **WHEN** the viewer is in two-page presentation and fit-to-page is active
- **THEN** the zoom is computed so both pages together fit inside the viewport

#### Scenario: Fit combined width to viewport
- **WHEN** the viewer is in two-page presentation and fit-to-width is active
- **THEN** the zoom is computed so the combined width of both pages equals the viewport width

### Requirement: Two-page partner pairing

The viewer SHALL pair pages as consecutive left/right pairs starting from an even page index, so that even-indexed pages pair with the following odd-indexed page. When the page count is odd, the last page has no partner.

#### Scenario: Consecutive pages pair
- **WHEN** the document has an even number of remaining pages in the current unit
- **THEN** page 2 pairs with page 3, page 4 with page 5, and so on

#### Scenario: Odd last page unpaired
- **WHEN** the document has an odd number of pages
- **THEN** the final page is shown without a partner
