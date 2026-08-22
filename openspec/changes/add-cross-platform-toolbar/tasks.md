# add-cross-platform-toolbar — Tasks

## 1. Engine search API (shared)

- [ ] 1.1 Extend `src/document.h`: add `TextMatch` (page + normalized `QRectF` rects) plus `virtual bool supportsSearch() const` and `virtual QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase)` with default no-capability implementations
- [ ] 1.2 Implement `searchText` in MuPDF engine using `fz_search_page` (quads → normalized rects, `fz_try`/`fz_catch`); report `supportsSearch() == true`; serialize all `fz_*` calls behind a per-engine mutex
- [ ] 1.3 Implement `searchText` in DjVu engine from `ddjvu_document_get_pagetext(DJVU_TXT_WORD)` word boxes filtered by term with optional case folding; `supportsSearch()` reflects whether any hidden text exists
- [ ] 1.4 Verify with a debug log path: load sample PDF and DJVU, log match counts and rects per test term; confirm empty-text DjVu pages yield zero matches without errors

## 2. Search orchestration + highlighting (shared)

- [ ] 2.1 Create `src/searchcontroller.*`: worker thread walking pages from the current page (wrapping), calling `engine->searchText`, emitting progressive results via callback; atomic cancel flag checked per page
- [ ] 2.2 Add search state and iteration to `ViewerController`: set term/matchCase (re-runs on toggle change), next/previous active-match with wrap, match-count accessors for "n / m" feedback, no-match indicator state
- [ ] 2.3 Paint highlight overlay in `renderVisiblePages`: translucent fills for all visible matches scaled from normalized rects, visually distinct style for the active match
- [ ] 2.4 Cancel and join the search thread synchronously in `closeDocument()` before engine teardown; clear all highlight state

## 3. Toolbar and sidebar shared core

- [ ] 3.1 Create `src/toolbar.h`: control-ID enum, abstract `ToolbarBackend` (create/show/enable/check, set text/icon, query edit text), and `ToolbarPresenter` mapping controller state → backend updates and backend events → controller calls; add `kToolbarBaseHeight` to `viewer_settings.h`
- [ ] 3.2 Update page-area geometry so both viewers subtract toolbar height (top) and info panel (bottom), scaled by dpiScale
- [ ] 3.3 Create `src/toolbar_icons.*`: programmatic monochrome glyphs (prev/next, first-page style arrows, mode toggle, fit manual/page/width, rotate L/R, zoom ±, find, print, copy, sidebar list/toggle) rendered at requested pixel size × dpiScale into HBITMAP (Windows) and QPixmap (Linux)
- [ ] 3.4 Create `src/sidebar.h` + `SidebarPresenter`: expansion/flattening of the `OutlineItem` tree from `DocumentEngine::outline()`, active-entry tracking fed by `trackCurrentPage`/`pageAtScrollOffset`, entry-activation → `goToPage`; extend geometry so page width subtracts the visible sidebar width (base × dpiScale)

## 4. Windows toolbar backend

- [ ] 4.1 Create `src/toolbar_win32.cpp`: child-HWND strip hosting BUTTON/EDIT/STATIC controls with manual DPI-aware layout and relayout on WM_DPICHANGED
- [ ] 4.2 Route native notifications through the presenter: navigation buttons, page edit commit (Enter/KillFocus, clamped), total-pages label, continuous-mode check state, fit-cycle button icon by FitMode, rotate, zoom enable/disable at limits
- [ ] 4.3 Host the toolbar in `ViewerWin32`, apply focus-neutrality rule in wndProc (viewer keys only when focus is outside toolbar edits), restore shortcuts after focus returns
- [ ] 4.4 Build windows-x64-release and manually smoke-test: navigate/type-jump/toggle modes/rotate/zoom with mouse only

## 5. Linux toolbar backend (Qt)

- [ ] 5.1 Create `src/toolbar_qt.cpp`: QWidget row with QToolButtons/QLineEdit/QLabels bound through the same presenter; checkable mode/fit buttons, editingFinished page jump
- [ ] 5.2 Host in the Qt viewer layout; verify identical behaviors as task 4.4 on a Linux build

## 6. Search UI (both platforms)

- [ ] 6.1 Wire find box, previous/next match buttons, and match-case toggle through the presenter; disable the group when `!supportsSearch()`; show "n / m" or no-match indication near the find box
- [ ] 6.2 Marshal SearchController callbacks to each platform's UI thread (custom posted message on Windows; queued invocation on Linux) and repaint so highlights appear progressively while searching
- [ ] 6.3 Verify spec scenarios: wrap-around iteration, cross-page next/previous, clear-box removes highlights, zoom keeps highlights aligned, typing in the box triggers no shortcuts

## 7. Copy button placeholder (both platforms)

- [ ] 7.1 Add the copy control to both backends via the presenter, permanently disabled, with tooltip documenting that text selection arrives from a future change
- [ ] 7.2 Verify on both platforms: button never enables for any document or interaction, no fallback action fires

## 8. Outline sidebar — Windows backend

- [ ] 8.1 Create `src/sidebar_win32.cpp`: child panel hosting WC_TREEVIEW with lazy per-level population and DPI-scaled sizing
- [ ] 8.2 Wire through SidebarPresenter: sidebar-toggle button shown only when outline is non-empty, entry activation navigates, highlighted entry follows keyboard/scroll navigation with path auto-expand
- [ ] 8.3 Apply the focus-neutrality rule to the tree control; verify resize keeps sidebar docked left without overlapping page area or panels

## 9. Outline sidebar — Linux backend (Qt)

- [ ] 9.1 Create `src/sidebar_qt.cpp`: fixed-width QTreeWidget dock area bound through the same presenter with lazy population
- [ ] 9.2 Verify identical behaviors as task 8.2–8.3 on a Linux build

## 10. Print coordination (shared)

- [ ] 10.1 Create `src/printcoordinator.*`: resolved job model (page list × copies from dialog output), render loop at printer resolution applying current rotation and fit-to-printable-area scaling, progress callback, cancel before spool starts; run loop on worker thread

## 11. Windows printing backend

- [ ] 11.1 Create `src/print_win32.cpp`: `PrintDlgEx` pre-filled with ..pageCount range → DEVMODE → printer DC; spool via StartDoc/StartPage + StretchDIBits honoring PHYSICALOFFSETX/Y margins
- [ ] 11.2 Error paths: no-printer and spool-failure show an error message without crashing; cancelling the dialog leaves viewer state (page, scroll, mode, highlights) untouched

## 12. Linux printing backend

- [ ] 12.1 Add `Qt6::PrintSupport` link for Linux-only targets in CMakeLists.txt
- [ ] 12.2 Create `src/print_qt.cpp`: QPrintDialog + QPrinter painter spooling through PrintCoordinator with the same error/cancel semantics as task 11.2

## 13. Verification and polish

- [ ] 13.1 Run every scenario in all three delta specs on both platforms; record gaps and fix
- [ ] 13.2 DPI pass: 100% and 200% — toolbar height, icon sharpness, sidebar dimensions, control layout after WM_DPICHANGED
- [ ] 13.3 Regression: all pre-existing hotkeys behave unchanged and keep chrome visuals synchronized (page box, mode button, fit icon, sidebar selection)
- [ ] 13.4 Outline pass: PDF with deep TOC (lazy expansion works), EPUB outline, DjVu without outline (toggle absent); confirm copy placeholder stays disabled throughout all passes
- [ ] 13.5 Update AGENTS.md architecture/conventions notes (toolbar/sidebar files, removed "no toolbar" convention, new Linux-only dependency, deferred copy binding)
