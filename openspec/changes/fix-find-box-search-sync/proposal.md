## Why

Issue #12: the toolbar find box and the committed search query are separate state, so they drift. Typing a new term leaves the previous term's highlights on screen, the Prev/Next buttons stay disabled until the first Enter, and once enabled they navigate the *old* result set while the box shows something else. The find box should be the single source of truth for the current term on both Windows and Linux.

## What Changes

- **BREAKING** (behavior): modifying the find box invalidates the current search — its highlights, match count, and active match are dropped immediately, on both platforms.
- Prev/Next are enabled whenever a searchable document is open and the box is non-empty (no longer gated on an existing result set).
- Activating Prev/Next with no active search for the current box text runs the search first (same semantics as Enter); with results already present they navigate as today.
- Prev on a fresh term selects the first match at or after the current page (page-granular, forward-biased, wrapping — unchanged selection semantics) and never scrolls when there are no matches.
- Opening a document or switching files refreshes find state: the box is cleared and no highlights remain.
- The match-case toggle re-runs the search only when a query is already active, and updates matches immediately; it never triggers a scan from an empty/never-committed search.
- Rapid Prev/Next activations during an in-flight scan are accepted as no-ops (no queued re-scans).

## Capabilities

### New Capabilities
(none)

### Modified Capabilities
- `viewer-text-search`: any box edit invalidates results; Prev/Next may initiate a search; document open resets find state; match-case re-evaluates only an active query.
- `viewer-toolbar`: Find Prev/Next enablement is driven by find-box content plus search capability, backed by a new edit-changed notification from both native backends.

## Impact

- `src/toolbar.*` (presenter rules + narrow find-control refresh)
- `src/toolbar_win32.cpp`, `src/toolbar_qt.cpp` (edit-changed signal; focus/guard handling)
- `src/viewercontroller.*` (search invalidation and query-triggered search)
- No engine, format, or rendering changes.
