## Context

See `proposal.md - Why`. Today the find box text lives only in the platform backend (`ToolbarWin32`/`ToolbarQt` edit control), while the "committed" term and its results live in `ViewerController` (`m_searchQuery`, `m_searchHits`). Only Enter (`returnPressed`/`VK_RETURN`) moves text across that boundary, so the two drift: editing the box leaves the previous term's highlights and the Prev/Next buttons navigate the stale result set (`toolbar.cpp:251-284`).

The search scan itself is already restartable: `SearchController` takes a generation guard, cancellation is checked between pages, and stale marshaled callbacks are dropped by comparing generations (`viewercontroller.cpp:1289-1322`). The Win32 UI marshal is `PostMessageW` (`viewer_win32.cpp:66`), so a worker never blocks on the UI thread and `join()` on the UI thread waits at most one page search — no deadlock. This change adds no new search machinery; it wires the find box into the existing lifecycle.

## Goals / Non-Goals

**Goals:**
- Make the find box the single source of truth for the current term on both platforms.
- Eliminate stale-highlight drift: any box edit drops the old result set immediately.
- Make Prev/Next usable as soon as the box is non-empty, and let them initiate the search.
- Keep selection semantics and the existing scan lifecycle unchanged.

**Non-Goals:**
- Live/debounced as-you-type searching, or per-keystroke scanning (would require a per-page stext cache and non-blocking cancel; `MuPdfEngine::searchText` rebuilds stext per call).
- Nearest-match selection. Selection stays page-granular and forward-biased (documented in `viewer-text-search`).
- Any engine, rendering, or format-dispatcher change.

## Decisions

### 1. The box is authoritative; an edit invalidates, but does not clear the box
`onFindTextChanged` calls `m_controller->clearSearch()` and refreshes only find controls. The box keeps its text; `clearSearch()` early-returns when nothing is active, so editing before any search is free (`viewercontroller.cpp:1267`).
Alternative considered: silently re-running the search on each edit. Rejected — it turns every keystroke into a whole-document scan with repeated stext extraction.

### 2. Search trigger keyed on the committed query, not the hit list
"Should this activation search first?" is answered by `boxText != searchQuery()` (or no query committed). Keying it on `hits.isEmpty()` would re-scan a zero-match term on every Prev/Next activation. Reusing `onFindCommitted` handles Next correctly via its cycle branch (`toolbar.cpp:262`), but Prev cannot reuse it because that branch always steps forward — Prev must branch on the same query comparison and call `startSearch` directly.

### 3. Enablement and a narrow refresh
Prev/Next enabled when `searchAvailable() && !boxText.trimmed().isEmpty()`; find box and match-case gate on `searchAvailable()` alone. The edit notification triggers a dedicated `updateFindControls()` rather than the ~20-control `refreshState()`, avoiding owner-drawn repaint churn at typing frequency.

### 4. One new backend → presenter signal
`ToolbarBackend` gains a find-text-changed notification. Win32 routes `EN_CHANGE` for the find edit (today filtered out at `toolbar_win32.cpp:665`); Qt connects `QLineEdit::textChanged` (today only `returnPressed` is connected, `toolbar_qt.cpp:189`). No behavior is added to the backend beyond raising the signal; the presenter owns all policy.

### 5. Match-case re-runs only an active query
`onMatchCaseToggled` keeps the flag, then `startSearch(boxText, on)` only when `!searchQuery().isEmpty()`. Toggling with an empty/never-searched box only sets the flag. This satisfies the existing "re-evaluate immediately" requirement without surprise-scanning, and matches `viewer-text-search`.

### 6. Document load resets find state
On open, the presenter clears the box (`setEditText(FindBox, "")`) and calls `clearSearch()`, so no term or highlights leak across documents. A re-entrancy guard makes the programmatic set inert.

### 7. Programmatic sets are guarded
Any future `setEditText(FindBox, …)` must not be treated as a user edit. A presenter/backer flag suppresses the change notification while the presenter writes the box.

### 8. IME composition
Win32 fires `EN_CHANGE` for each composition step. Because this change never auto-searches on edit, intermediate compositions only trigger cheap invalidations of an already-empty result set. If auto-search is ever added, it must gate on composition state.

### 9. Rapid clicks during a scan are swallowed
First activation sets the query and starts the async scan; a second activation hits the already-active branch and no-ops on an empty/partial list. Accepted rather than queued. The status label already shows "searching".

## Risks / Trade-offs

- [Every edit calls `clearSearch()` → `cancel()+join()` on the UI thread] → Bounded to one page search; marshal is `PostMessage` (no deadlock). Guarded early-return when idle. Acceptable for a single lister.
- [Aggressive clearing: one stray keystroke wipes all highlights] → Intended by "the box is the term"; documented behavior. Reverting would reintroduce drift.
- [Match-case re-run resets the active match to the current page] → Spec-consistent ("nearest remaining"); page-granular selection is documented, not changed.
- [No-match term leaves Prev/Next enabled but inert] → Accepted; avoids re-scan storms (decision 2).
- [Doc-load box clear could surprise a user carrying a term between files] → Chosen over silent cross-document search.

## Platform-specific code

| Concern | Windows (`toolbar_win32.cpp`) | Linux (`toolbar_qt.cpp`) |
|---|---|---|
| Edit-changed signal | Handle `EN_CHANGE` for `ID_FIND_EDIT` in the existing `WM_COMMAND` branch instead of discarding it | `connect(QLineEdit::textChanged, …)` on the find box |
| Clear box on load | `SetWindowTextW`/`setEditText` with the re-entrancy guard | `QLineEdit::setText("")` with the guard |
| Commit remains | `VK_RETURN` in `editProc` (unchanged) | `returnPressed` (unchanged) |

Shared policy (invalidation, trigger comparison, enablement, match-case) lives entirely in `ToolbarPresenter`; the two backends only raise the edit signal and execute the guard.

## Migration Plan

Not a data/format change. No rollback beyond reverting the commit. The behavioral change is user-visible and intentional; no settings migration.

## Open Questions

None — all scoped decisions are resolved. Nearest-match selection and live search are explicitly deferred (Non-Goals).
