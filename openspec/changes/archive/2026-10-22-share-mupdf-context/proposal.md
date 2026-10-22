# Proposal

## Why

Opening the same MuPDF-backed file (PDF, EPUB, MOBI, CHM, CBZ, TIFF) in two
concurrent Lister windows fails: the first window opens in our plugin, the
second falls through to the next WLX plugin. Forcing the plugin manually can
still show the file twice, so the host protocol is not the cause.

Reproduced in-process (two `ListLoadW` calls on one loaded `MultidocViewer.wlx64`,
the same shape as two TC Listers sharing the DLL): the second open returns
`NULL` and the debug output shows

```
argument error: Stock LCMS2 library cannot be used in multiple contexts!
cannot create context (phase 2)
```

Each engine instance (`MuPdfEngine`, `ChmEngine`) owns a private `fz_context`
via `fz_new_context()`, but the statically linked stock lcms2 (ICC backend,
see `add-icc-color-management`) allows only one live context per process.
DjVu files open twice cleanly because `DjVuEngine` never touches MuPDF.

## What Changes

- Add a process-wide shared MuPDF context (`src/mupdfcontext.h`, header-only so
  the plugin and every harness pick it up with no CMake changes), created once
  and never destroyed.
- `MuPdfEngine` and `ChmEngine` attach to the shared context instead of
  creating/dropping their own; `close()` drops only their own documents and
  archives. All `fz_*` calls serialize on one global mutex replacing the
  per-instance ones (UI renders plus search/print worker threads now share the
  context across engine instances).
- On Windows, `ChmEngine` always stages a private per-window temp copy:
  libchm's narrow `chm_open()` passes `dwShareMode=0` to `CreateFileA`, so an
  in-place first open locks the file exclusively and a second window fails.
  (Linux opens in place via `open(O_RDONLY)` and is unaffected.)
- Scope the CHM stylesheet per topic document (`fz_style_document` where the
  capability probe allows it) instead of context-wide `fz_set_user_css`, so CHM
  rules cannot leak into reflowable documents opened by other windows on the
  shared context. On older system libmupdf without per-document styling, set the
  context CSS at the single topic-open choke point while the global lock is
  held (every parse/layout site sets its own CSS first — same discipline the
  MuPDF engine already follows).
- Keep ICC color management behavior unchanged (`viewer-rendering` ICC
  scenarios still hold) and keep the second-window failure fixed: same file
  twice and two different MuPDF files concurrently.

## Capabilities

### New Capabilities
<!-- None -->

### Modified Capabilities

- `viewer-rendering`: concurrent MuPDF-backed documents share one engine
  context (new requirement + scenarios below).

## Impact

- Affected files: `src/mupdfcontext.h` (new), `src/mupdfengine.*`,
  `src/chmengine.*`, `tests/harness_fb2zip.cpp` (stale "sequential contexts"
  comment), `AGENTS.md` (Fixed entry).
- No packaging, INI, detect-string, or WLX API changes; no CMake changes.
