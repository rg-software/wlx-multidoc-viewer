## Why

Continuous mode currently only supports keyboard and mouse wheel for scrolling. Users expect to click-and-drag to pan the document, matching the interaction model of SumatraPDF and every PDF viewer. Additionally, scroll step sizes, panel heights, and margins are scattered as magic numbers across the Win32 viewer code — extracting them into a single settings header centralises tuning and eliminates duplicated values (e.g., info panel height "22" in three places).

## What Changes

- **Mouse drag panning**: Left-button drag in continuous mode scrolls the viewport proportional to mouse movement (both axes). Cursor changes to a grabbing hand during drag.
- **Settings header**: A new `viewer_settings.h` consolidates all tuning constants (scroll steps, panel height, margins, buffer pages, etc.) into one file. All viewer files include it instead of hardcoding values.

## Capabilities

### New Capabilities
- `viewer-interaction/mouse-drag`: Mouse drag to pan document content in continuous mode (Win32 and Qt viewers).

### Modified Capabilities
- `viewer-rendering`: Extract magic numbers to settings header (no spec-level behaviour change, but constants centralised).
- `viewer-info-panel`: Info panel height sourced from settings header instead of duplicated literals.

## Impact

- `src/viewer_win32.cpp` / `src/viewer_win32.h`: Add `WM_LBUTTONDOWN`, `WM_LBUTTONUP`, `WM_MOUSEMOVE` handlers, cursor management, drag state tracking.
- `src/viewer.cpp` / `src/viewer.h` (Qt): Add `mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent` equivalents.
- `src/viewercontroller.cpp` / `src/viewercontroller.h`: Remove duplicated `kInfoPanelHeight` constant.
- New file `src/viewer_settings.h`: Centralised constants.
