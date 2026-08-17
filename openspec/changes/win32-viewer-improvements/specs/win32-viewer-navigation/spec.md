## Purpose

Provides keyboard and scroll-based page navigation for multi-page documents in the Win32 viewer, letting users move forward, backward, and jump to specific pages.

## ADDED Requirements

### Requirement: Page navigation commands
The viewer SHALL support navigating to the next page, previous page, first page, and last page of a multi-page document.

#### Scenario: Navigate to next page
- **WHEN** the user presses the Right arrow key or Page Down
- **THEN** the viewer displays the next page if one exists

#### Scenario: Navigate to previous page
- **WHEN** the user presses the Left arrow key or Page Up
- **THEN** the viewer displays the previous page if one exists

#### Scenario: Navigate to first page
- **WHEN** the user presses the Home key
- **THEN** the viewer displays page 1

#### Scenario: Navigate to last page
- **WHEN** the user presses the End key
- **THEN** the viewer displays the last page

### Requirement: Page bounds enforcement
The viewer SHALL NOT allow navigation beyond the first or last page.

#### Scenario: At first page
- **WHEN** the viewer is on page 1 and the user presses Left or Page Up
- **THEN** the viewer remains on page 1

#### Scenario: At last page
- **WHEN** the viewer is on the last page and the user presses Right or Page Down
- **THEN** the viewer remains on the last page

### Requirement: Page indicator
The viewer SHALL display the current page number and total page count in the status area.

#### Scenario: Page indicator updates on navigation
- **WHEN** the user navigates to a different page
- **THEN** the page indicator updates to show "current/total"
