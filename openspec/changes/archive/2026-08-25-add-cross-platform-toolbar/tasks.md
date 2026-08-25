# add-cross-platform-toolbar — Tasks

## 1. Engine search API (shared)

- [x] 1.1 Extend `src/document.h`: add `TextMatch` (page + normalized `QRectF` rects) plus `virtual bool supportsSearch() const` and `virtual QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase)` with default no-capability implementations
- [x] 1.2 Implement `searchText` in MuPDF engine using `fz_search_page_cb` (quads → normalized rects, case-sensitive fallback walks stext chars with per-char geometry, `fz_try`/`fz_catch`); report `supportsSearch() == true`; serialize all `fz_*` calls behind a per-engine mutex
- [x] 1.3 DjVu search: `supportsSearch() == false` (no-capability) — deviation agreed during implementation: the vcpkg static djvulibre build does not export the miniexp tree-walking accessors required to read `ddjvu_document_get_pagetext` (see AGENTS.md open gaps), so word-box extraction is not linkable; find controls are disabled for DjVu
- [ ] 1.4 Verify with a debug log path: load sample PDF and DJVU, log match counts and rects per test term; confirm empty-text DjVu pages yield zero matches without errors

## 2. Search orchestration + highlighting (shared)

- [x] 2.1 Create `src/searchcontroller.*`: worker thread walking pages from the current page (wrapping), calling `engine->searchText`, emitting progressive results via callback; atomic cancel flag checked per page
- [x] 2.2 Add search state and iteration to `ViewerController`: set term/matchCase (re-runs on toggle change), next/previous active-match with wrap, match-count accessors for "n / m" feedback, no-match indicator state
- [x] 2.3 Paint highlight overlay in the paint paths of both viewers (`paintSearchOverlay` Win32, `paintSearchOverlay` Qt canvas): translucent fills for all visible matches scaled from normalized rects, visually distinct style for the active match
- [x] 2.4 Cancel and join the search thread synchronously in `closeDocument()` before engine teardown; clear all highlight state (generation-guarded so stale marshaled results are ignored)

## 3. Toolbar and sidebar shared core

- [x] 3.1 Create `src/toolbar.h` (+`toolbar.cpp`): control-ID enum, abstract `ToolbarBackend`, and `ToolbarPresenter` mapping controller state → backend updates and backend events → controller calls; added `kToolbarBaseHeight` / `kSidebarBaseWidth` / `kIconBaseSize` to `viewer_settings.h`
- [x] 3.2 Update page-area geometry so both viewers subtract toolbar height (top: `setTopChrome`), info panel (bottom: `setBottomChrome`), and optional sidebar (left: `setLeftChrome`), all scaled by dpiScale; info bar moved to the bottom in both viewers
- [x] 3.3 Create `src/toolbar_icons.*`: programmatic monochrome glyphs (prev/next, mode, fit manual/page/width, rotate L/R, zoom ±, find, match-case, print, copy, sidebar list/toggle) rendered at requested pixel size × dpiScale into HBITMAP (Windows backend) and QPixmap (Qt backend)
- [x] 3.4 Create `src/sidebar.h` + `SidebarPresenter`: expansion/flattening of the `OutlineItem` tree with pre-order ids, active-entry tracking via reading position, entry-activation → navigation, lazy `childrenOf()` for per-level population, geometry extends page width by the visible sidebar width

## 4. Windows toolbar backend

- [x] 4.1 Create `src/toolbar_win32.*`: child-HWND strip hosting owner-drawn BUTTON, EDIT, STATIC controls with manual DPI-aware layout and relayout on WM_DPICHANGED/WM_SIZE; built-in tooltips
- [x] 4.2 Route native notifications through the presenter: navigation buttons, page edit commit (Enter/KillFocus, clamped), total-pages label, continuous-mode check state, fit-cycle button icon by FitMode, rotate, zoom enable/disable at limits
- [x] 4.3 Host the toolbar in `ViewerWin32`, apply focus-neutrality rule in wndProc (viewer keys only when focus is outside toolbar edits; `isEditFocused()` guard), shortcuts restored when focus returns
- [ ] 4.4 Build windows-x64-release and manually smoke-test: navigate/type-jump/toggle modes/rotate/zoom with mouse only (Win32 build compiles; interactive run still to be done on a device)

## 5. Linux toolbar backend (Qt)

- [x] 5.1 Create `src/toolbar_qt.*`: QWidget row with QToolButtons / QLineEdits / QLabels bound through the same presenter; checkable mode/fit/sidebar buttons (match-case toggle re-runs search), `editingFinished` page jump
- [ ] 5.2 Verify identical behaviors as task 4.4 on a Linux build (code built only on Linux; run still required)

## 6. Search UI (both platforms)

- [x] 6.1 Wire find box, previous/next match buttons, and match-case toggle through the presenter; disable the group when `!supportsSearch()`; show "n / m" or "No matches" near the find box; clear-box removes highlights
- [x] 6.2 Marshal SearchController callbacks to each platform's UI thread (custom posted message `WM_PLUGIN...` on Windows, `QueuedConnection` on Linux) and repaint so highlights appear progressively while searching
- [ ] 6.3 Verify spec scenarios: wrap-around iteration, cross-page next/previous, clear-box removes highlights, zoom keeps highlights aligned, typing in the box triggers no shortcuts

## 7. Copy button placeholder (both platforms)

- [x] 7.1 Add the copy control to both backends via the presenter, permanently disabled, with tooltip documenting that text selection arrives from a future change
- [ ] 7.2 Verify on both platforms: button never enables for any document or interaction, no fallback action fires

## 8. Outline sidebar — Windows backend

- [x] 8.1 Create `src/sidebar_win32.*`: child panel hosting WC_TREEVIEW with per-level lazy population (placeholder children, materialized on expand) and DPI-scaled sizing
- [x] 8.2 Wire through SidebarPresenter: sidebar-toggle appears only when outline non-empty, entry activation navigates, highlighted entry follows keyboard/scroll navigation with path auto-expand
- [ ] 8.3 Apply the focus-neutrality rule to the tree control (tree is a separate child window so keys route to it natively); verify resize keeps sidebar docked left without overlapping page area or panels

## 9. Outline sidebar — Linux backend (Qt)

- [x] 9.1 Create `src/sidebar_qt.*`: fixed-width QTreeWidget dock area bound through the same presenter with lazy population (itemExpanded materialization, ensure-path for selection)
- [ ] 9.2 Verify identical behaviors as task 8.2–8.3 on a Linux build

## 10. Print coordination (shared)

- [x] 10.1 Create `src/printcoordinator.*`: resolved job model (page list × copies), render loop at printer resolution applying current rotation and fit-to-printable-area scaling (same math on both platforms), progress callback, cancel before spool starts; runs on a worker thread with a per-page sink

## 11. Windows printing backend

- [x] 11.1 Create `src/print_win32.cpp`: `PrintDlgEx` pre-loaded with the document page range → DEVMODE → printer DC; spool via StartDoc/StartPage, pages re-rendered at printer resolution and StretchDIBits'd honoring PHYSICALOFFSETX/Y margins
- [x] 11.2 Error paths: no-printer and spool-failure show an error message without crashing; cancelling the dialog leaves viewer state (page, scroll, mode, highlights) untouched

## 12. Linux printing backend

- [x] 12.1 Add `Qt6::PrintSupport` link for Linux-only targets in CMakeLists.txt
- [x] 12.2 Create `src/print_qt.cpp`: QPrintDialog + QPrinter painter spooling through the shared render math with the same error/cancel semantics as task 11.2 (Qt requires the printer used on the main thread; the page/copy list resolution is shared)

## 13. Verification and polish

- [ ] 13.1 Run every scenario in all three delta specs on both platforms; record gaps and fix
- [ ] 13.2 DPI pass: 100% and 200% — toolbar height, icon sharpness, sidebar dimensions, control layout after WM_DPICHANGED
- [ ] 13.3 Regression: all pre-existing hotkeys behave unchanged and keep chrome visuals synchronized (page box, mode button, fit icon, sidebar selection)
- [ ] 13.4 Outline pass: PDF with deep TOC (lazy expansion works), EPUB outline, DjVu without outline (toggle absent); confirm copy placeholder stays disabled throughout all passes
- [x] 13.5 Update AGENTS.md architecture/conventions notes (toolbar/sidebar files, removed "no toolbar" convention, new Linux-only `Qt6::PrintSupport` dependency, deferred copy binding; DjVu search unchanged in AGENTS.md open gaps)