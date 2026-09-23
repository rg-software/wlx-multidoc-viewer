## 1. Shared: presenter and controller policy

- [ ] 1.1 Add a find-text-changed notification to the `ToolbarBackend` contract (`src/toolbar.h`) and route it to `ToolbarPresenter::onFindTextChanged()`; no-op for backends until wired.
- [ ] 1.2 Implement `onFindTextChanged()`: keep the box text, call `m_controller->clearSearch()`, then `updateFindControls()`; honor a re-entrancy guard so programmatic box writes are inert (design decisions 1 and 7).
- [ ] 1.3 Add `ToolbarPresenter::updateFindControls()` computing `FindPrev`/`FindNext` enablement as `searchAvailable() && !boxText.trimmed().isEmpty()`, and use it instead of full `refreshState()` on edits (design decision 3).
- [ ] 1.4 Rework `onFindNext()`/`onFindPrev()`: when `boxText != controller->searchQuery()` (or no query), call `startSearch(boxText, m_matchCase)` and return; otherwise call `nextMatch`/`prevMatch` as today. Do not route Prev through `onFindCommitted` (design decision 2).
- [ ] 1.5 Update `onMatchCaseToggled()` to re-run `startSearch(boxText, on)` only when `!controller->searchQuery().isEmpty()`; otherwise only set the flag and refresh (design decision 5).
- [ ] 1.6 Reset find state on document load: clear the box via the backend and call `clearSearch()`, guarded against the change signal (design decision 6).
- [ ] 1.7 Ensure `onFindCommitted` empty-term path and the Enter re-cycle path still behave; refresh find controls after each state change.

## 2. Windows backend (Win32)

- [ ] 2.1 In `ToolbarWin32::handleMsg` `WM_COMMAND`, route `EN_CHANGE` for `ID_FIND_EDIT` to the presenter instead of discarding it (`src/toolbar_win32.cpp:663-671`).
- [ ] 2.2 Add the re-entrancy guard around programmatic find-box `SetWindowTextW` in `setEditText` so presenter-driven clears do not raise edits.
- [ ] 2.3 Clear the find box on document open/switch through the guarded setter.

## 3. Linux backend (Qt)

- [ ] 3.1 Connect `QLineEdit::textChanged` on the find box to the presenter (`src/toolbar_qt.cpp`), keeping `returnPressed` for Enter commit.
- [ ] 3.2 Add the re-entrancy guard around programmatic find-box `setText` so presenter-driven clears do not raise edits.
- [ ] 3.3 Clear the find box on document open/switch through the guarded setter.

## 4. Regression harness

- [ ] 4.1 Add `tests/harness_findbox.cpp` with a fake `ToolbarBackend` driving a real `ToolbarPresenter` + minimal `ViewerController`: assert buttons enable on first non-empty box character, disable on empty, and that Prev/Next with no active query invoke a search while with an active query they navigate.
- [ ] 4.2 Assert that an edit clears highlights/count and that a zero-match term does not re-trigger a scan on repeated Prev/Next activation (trigger-condition regression).

## 5. Verification (both platforms)

- [ ] 5.1 Windows: build with `cmake --preset windows-x64-release`; confirm issue #12 repro 1 (type `bar`, Enter, type `foo`, click `>` highlights `foo`) and repro 2 (buttons enabled after first character) in Total/Double Commander.
- [ ] 5.2 Linux: build with `cmake --preset linux-release`; confirm the same two repros in a file manager.
- [ ] 5.3 Regression pass on both: Enter search + wrap, highlight anchor across zoom/rotate, match-case toggle with and without an active query, rapid Prev/Next during a scan, and document switch clearing the box and highlights.
