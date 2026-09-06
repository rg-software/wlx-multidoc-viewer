## Why

The outline sidebar (issue #1) is currently a fixed 180px strip: long TOC entries are cut off and there is no way to widen it, so readable outlines are impossible for documents with deep or long section titles. It also does not force its horizontal scroll back to the left edge after navigation, so after scrolling a long entry into view the beginnings of lines can be hidden.

## What Changes

- The sidebar tree SHALL reset its horizontal scroll to the left edge whenever it brings an entry into view (selection sync, navigation, entry reload), so overlong titles show their beginnings. The user can still scroll horizontally afterward.
- The sidebar SHALL be resizable by dragging its right edge, on both Windows and Linux, widening/narrowing within documented bounds. The page area relayouts live during the drag.
- The adjusted width is **session-only**: it persists across document loads inside the same viewer window and resets to the default 180px when a new viewer window is created. Nothing is written to disk.
- The issue's third point (open/closed by default from the ini file) is **deferred**: the user explicitly chose to tackle ini support later. No ini reading or writing is introduced by this change; the sidebar keeps its current behavior of starting hidden per document load.

## Capabilities

### New Capabilities
- none

### Modified Capabilities
- `viewer-toolbar`: the outline sidebar gains a drag-to-resize control with bounded, session-scoped width, and its tree always reveals line beginnings by resetting horizontal scroll to the left edge when entries enter view.

## Impact

- `viewer_settings.h` — shared sidebar width bounds and resize-grip constants.
- `sidebar_win32.*` / `sidebar_qt.*` — right-edge drag handle, width tracking (logical px, DPI-scaled for screen), horizontal-scroll reset on entry reveal.
- `viewer_win32.*` / `viewer.*` — wire width changes through the existing chrome chain (`setLeftChrome` → `relayout`) and reset tree horizontal scroll on show/reload.
- Both platforms share identical behavior; only the native widget plumbing differs.