## Why

The viewer currently shows one page at a time (paged) or a continuous vertical strip, with no way to view two adjacent pages side by side. Book-like documents read more naturally as a two-page spread, which is a common expectation in document viewers. We need a way to pick how pages are presented — one page, plain two pages, or two pages with the first page treated as a cover — while preserving the existing paged/continuous and fit behaviors.

## What Changes

- Add a **page-presentation** control with **three states**, separate from the existing paged/continuous mode toggle. The new control sits between the mode toggle and the fit button on the toolbar.
- The three states are:
  - **Single page** — one page per view unit (today's behavior).
  - **Double page** — pages group in consecutive pairs: (1 2), (3 4), (5 6), …
  - **Double page / cover** — the first page gets its own dedicated area, then the remaining pages pair consecutively: (1), (2 3), (4 5), …
- In a **double-page** state (either variant):
  - **Paged mode** displays the pages of the current unit at once instead of one.
  - **Continuous mode** scrolls a unit's pages as a single logical unit (they advance together).
  - **Fit modes** target the unit: fit both pages together, fit-to-width across the unit, or fit both into the screen.
- The **single-page** state behaves exactly as today.
- For double page, when a unit has only one member (cover page 1, or the odd trailing page of an odd-count remainder), that page is shown alone.

## Capabilities

### New Capabilities
- `viewer-display-modes/two-page-view`: behavior of the double-page presentation states (plain and cover-aware) across paged/continuous display and fit modes.

### Modified Capabilities
- `viewer-display-modes`: display-mode requirements gain the concepts of a second presentation axis and cover-aware pairing; existing paged/continuous behavior is preserved for single-page presentation and extended for double-page.
- `viewer-toolbar`: a new page-presentation control (single-page / double-page / double-page-with-cover) is inserted between the display-mode toggle and the fit button, with its state reflecting the active presentation on both platforms.

## Impact

- `viewercontroller.*` — `ViewerState`/`ViewerController` gain a three-state page-presentation setting; `computeLayout`, `computeFitZoom`, `relayout`, navigation, and page-tracking consider the pairing rule (cover-aware or plain).
- `viewer_win32.*` / `viewer.*` — paint, scroll, and keyboard paths add double-page branches (cover-aware where relevant) in both paged and continuous modes on Windows and Linux.
- `toolbar.*` / `toolbar_win32.*` / `toolbar_qt.*` — new control + icon wiring for three states.
- Both platforms are affected equally.
