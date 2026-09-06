## Why

The viewer currently shows one page at a time (paged) or a continuous vertical strip, with no way to view two adjacent pages side by side. Book-like documents read more naturally as a two-page spread, which is a common expectation in document viewers. We need a way to toggle between one-page and two-page presentation while preserving the existing paged/continuous and fit behaviors.

## What Changes

- Add a new **page-presentation** toggle (one-page vs two-page), separate from the existing paged/continuous mode toggle. The new button sits between the mode toggle and the fit button on the toolbar.
- In **two-page** presentation:
  - **Paged mode** displays two adjacent pages at once instead of one.
  - **Continuous mode** scrolls two pages as a single logical unit (both advance together).
  - **Fit modes** target both pages: fit both pages together, fit-to-width across both pages, or fit both into the screen.
- One-page presentation behaves exactly as today.
- When the page count is odd, the final page is shown alone (the trailing page has no partner).

## Capabilities

### New Capabilities
- `viewer-display-modes/two-page-view`: behavior of the two-page presentation mode across paged/continuous display and fit modes.

### Modified Capabilities
- `viewer-display-modes`: display-mode requirements gain the concept of a second presentation axis; existing paged/continuous behavior is preserved for one-page presentation and extended for two-page.
- `viewer-toolbar`: a new page-presentation toggle button (one-page / two-page) is inserted between the display-mode toggle and the fit button, with its pressed state reflecting the active presentation on both platforms.

## Impact

- `viewercontroller.*` — `ViewerState`/`ViewerController` gain a two-page presentation flag; `computeLayout`, `computeFitZoom`, `relayout`, navigation, and page-tracking consider paired pages.
- `viewer_win32.*` / `viewer.*` — paint, scroll, and keyboard paths add two-page branches in both paged and continuous modes on Windows and Linux.
- `toolbar.*` / `toolbar_win32.*` / `toolbar_qt.*` — new control + icon wiring.
- Both platforms are affected equally.
