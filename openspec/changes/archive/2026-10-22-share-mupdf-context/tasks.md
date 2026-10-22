# Tasks

## 1. Shared MuPDF context

- [x] 1.1 Add `src/mupdfcontext.h` (header-only `sharedMuPdfMutex()` /
  `sharedMuPdfContext()`, created once, never destroyed, documented locking
  contract); no CMake changes
- [x] 1.2 In `MuPdfEngine`, attach `m_ctx` to the shared context in
  `makeContext()` (fail cleanly when null), detach without
  `fz_drop_context` in `dropDocument()`, and swap every `m_mutex` guard to
  the global lock; update the "caller must hold" comments
- [x] 1.3 In `ChmEngine`, attach `m_fzCtx` to the shared context in `open()`
  (fail cleanly when null), detach without `fz_drop_context` in
  `dropArchive()`, and swap every `m_mutex` guard to the global lock; update
  the "caller must hold" comments

## 2. CHM stylesheet scoping and concurrent opens (Windows)

- [x] 2.1 In `openTopicDoc()`, apply `chmStylesheet()` per topic document via
  `fz_style_document(doc, 0, css)` after opening and before layout when
  `MUPDF_HAVE_STYLE_DOCUMENT`; otherwise set the context user CSS immediately
  before parsing (same global lock held)
- [x] 2.2 Remove the one-time `fz_set_user_css` from `ChmEngine::open()`
- [x] 2.3 Always stage a per-window temp copy in `ChmEngine::open()` on
  Windows (libchm narrow `chm_open` opens exclusive; raw Win32 I/O — a
  just-closed Qt handle still reads as locked); verified two concurrent
  windows on the same CHM plus CHM + PDF at once, temp cleaned on close

## 3. Verification

- [x] 3.1 Rebuilt (`windows-x64-release` + `windows-release` presets) and ran
  `harness-icc`, `harness-epub-count`, `harness-links` (PDF + 2 CHMs),
  `harness-mobi`, `harness-fb2zip`, `harness-search`, `harness-reflow`
  (mobi keeps its 2 pre-existing pagination failures, identical on pristine
  code — environmental, unrelated)
- [x] 3.2 Re-ran the two-`ListLoadW` double-open reproduction: same file twice
  for PDF/EPUB/CHM/DjVu/CBZ/MOBI (12/12) plus five concurrent mixed windows
  with close/reopen, no lcms2 diagnostics, no temp leftovers
- [x] 3.3 Fixed the stale "sequential contexts" comment in
  `tests/harness_fb2zip.cpp` and recorded the `viewer-rendering` spec delta +
  `AGENTS.md` Fixed entry
