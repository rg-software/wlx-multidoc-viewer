# Viewer Navigation Specification

## Purpose

Defines how the viewer moves between pages of a multi-page document, including the viewport-preserving next/prev behavior in continuous mode. The page indicator is owned by `viewer-info-panel` and is not duplicated here. Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## Requirements

### Requirement: Navigate to next and previous page

The viewer SHALL provide commands to advance to the next page and to return to the previous page of the open document. The semantics of "next/prev" depend on the active display mode and are defined in `viewer-display-modes`.

#### Scenario: Advance from a middle page in paged mode
- **WHEN** the viewer is in paged mode on a page that is neither the first nor the last and a "next page" command is issued
- **THEN** the viewer displays the next page with the scroll position reset to the top

#### Scenario: Go back from a middle page in paged mode
- **WHEN** the viewer is in paged mode on a page that is neither the first nor the last and a "previous page" command is issued
- **THEN** the viewer displays the previous page with the scroll position reset to the top

### Requirement: Navigate to first and last page

The viewer SHALL provide commands to jump directly to the first and to the last page of the open document.

#### Scenario: Jump to first page
- **WHEN** a "first page" command is issued from any page other than page 1
- **THEN** the viewer displays page 1

#### Scenario: Jump to last page
- **WHEN** a "last page" command is issued from any page other than the last page
- **THEN** the viewer displays the last page of the document

### Requirement: Clamp navigation at document bounds

The viewer SHALL NOT navigate beyond the first or last page. Issuing a navigation command that would move past either bound MUST leave the current page unchanged.

#### Scenario: Next at last page
- **WHEN** the viewer is on the last page and a "next page" command is issued
- **THEN** the viewer remains on the last page

#### Scenario: Previous at first page
- **WHEN** the viewer is on page 1 and a "previous page" command is issued
- **THEN** the viewer remains on page 1

### Requirement: Direct page jump

The viewer SHALL accept a target page index and display that page when the index is within range.

#### Scenario: Jump to a valid page
- **WHEN** the viewer receives a target page index that is within `[1, pageCount]`
- **THEN** the viewer displays that page

#### Scenario: Jump to an out-of-range page
- **WHEN** the viewer receives a target page index outside `[1, pageCount]`
- **THEN** the viewer rejects the request and the current page is unchanged

### Requirement: Continuous-mode next/prev preserves viewport position

In continuous mode, the "next page" and "previous page" commands SHALL advance the document by exactly one page without scrolling the viewport, so the same vertical slice of the viewport that showed the current page continues to show the next page.

#### Scenario: Next from split view
- **WHEN** the viewer is in continuous mode and the viewport shows the bottom 1/3 of page N and the top 2/3 of page N+1, and a "next page" command is issued
- **THEN** the viewport shows the bottom 1/3 of page N+1 and the top 2/3 of page N+2, with no scroll animation

#### Scenario: Previous from split view
- **WHEN** the viewer is in continuous mode and the viewport shows a split between page N and page N-1, and a "previous page" command is issued
- **THEN** the viewport shifts up by exactly one page with no scroll animation

#### Scenario: Clamped at document bounds in continuous mode
- **WHEN** the viewer is in continuous mode on the first or last page and the corresponding nav command is issued
- **THEN** the current page is unchanged (same bounds as paged mode)

### Requirement: Provide a "go to page" input on Linux

On Linux the viewer SHALL provide a dialog that prompts for a target page index and routes it to the direct-jump command. On Windows the viewer SHALL also expose the dialog when triggered programmatically (e.g. via `ListSendCommand`).

#### Scenario: Linux go-to-page dialog
- **WHEN** the user triggers the go-to-page command on Linux
- **THEN** an input dialog appears with the current page preselected and a numeric range of `[1, pageCount]`