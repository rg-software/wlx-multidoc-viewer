# viewer-text-search delta

## Purpose

Lets users find text across the entire document, jump between matches in order, and see every match highlighted on the rendered pages, with an optional case-sensitivity constraint.

## ADDED Requirements

### Requirement: Whole-document search
Committing a search term in the toolbar find box (Enter or the search action) SHALL search the text of all pages in the document, starting at the current page and wrapping to the beginning, and SHALL select the first match at or after the current reading position.

#### Scenario: First match selection
- **WHEN** the user enters a term that occurs later in the document and commits the search
- **THEN** the viewer navigates to that page and highlights the first occurrence at or after the current position

#### Scenario: Search wraps
- **WHEN** the only occurrences lie before the current page
- **THEN** the viewer wraps around and selects the first occurrence from the document start

### Requirement: Match iteration
The next-match and previous-match controls SHALL move the active match forward or backward through the full match list, wrapping at the document boundaries. Each activation SHALL bring the active match's location into view, switching pages in paged mode and scrolling in continuous mode.

#### Scenario: Next match crosses page boundary
- **WHEN** the active match is the last on its page and the user activates next-match
- **THEN** the viewer scrolls or flips so that the first match on the following page becomes the active highlighted match

#### Scenario: Previous wraps at start
- **WHEN** the active match is the first in the document and the user activates previous-match
- **THEN** the last match in the document becomes active and is brought into view

### Requirement: Match highlighting
The viewer SHALL highlight every match on visible pages while a search is active, and SHALL render the active match with a visually distinct style from other matches. Highlights SHALL remain anchored to their text when the page is re-rendered at different zoom, rotation, or DPI scale. Removing the search term or closing the document SHALL clear all highlights.

#### Scenario: Highlight survives zoom change
- **WHEN** matches are highlighted and the user zooms or rotates the view
- **THEN** the highlight rectangles stay aligned over their matching text

#### Scenario: Clearing removes highlights
- **WHEN** the user clears the find box
- **THEN** no highlight rectangles remain on any page

### Requirement: Case sensitivity option
The match-case toggle SHALL constrain matching to case-sensitive comparisons when enabled; by default it is off and matching ignores letter case. Changing the toggle SHALL re-evaluate the current search term immediately and update matches and highlighting accordingly.

#### Scenario: Toggle changes results
- **WHEN** a case-insensitive search shows a match and the user enables match-case such that the occurrence no longer matches
- **THEN** that occurrence loses its highlight and the active match moves to the nearest remaining match

### Requirement: Search feedback
The viewer SHALL indicate how many matches were found and which match is active (for example "3 / 17") near the find box while a search is active. When the term has no matches, the viewer SHALL show an explicit no-match indication instead of silently doing nothing.

#### Scenario: No matches
- **WHEN** the user searches for a term that does not occur in the document
- **THEN** the viewer shows a no-match indication and the current view stays unchanged

### Requirement: Engine text-layer support
Search SHALL be available for every format whose engine exposes a text layer with positional information. Engines without positional text SHALL report no capability, and the viewer disables find controls for those documents rather than failing.

#### Scenario: Capability detection
- **WHEN** the viewer opens a searchable PDF and then a format whose engine lacks positional text
- **THEN** find controls are enabled for the PDF and disabled for the other document
