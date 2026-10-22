# Design

## Context

See `proposal.md`. WLX hosts load the plugin DLL once per process and call
`ListLoad` once per Lister window, so two windows on MuPDF-backed files mean
two live engine instances — and, before this change, two live `fz_context`s —
in one process. Stock lcms2 (linked for ICC in `add-icc-color-management`)
refuses the second context, so the second `open()` fails and the host moves on
to the next plugin. MuPDF documents are cheap context-sharers (one context
routinely serves many documents); only the lcms2-backed context creation is
singular.

## Goals / Non-Goals

**Goals:**
- The same MuPDF-backed file opens in any number of concurrent windows; two
  different MuPDF-backed files also coexist.
- ICC color management keeps working exactly as specified (no `FZ_ENABLE_ICC`
  revert, no lcms2 swap, no re-verification of color output).
- No CSS/theming cross-talk between documents sharing the context.
- Threading stays correct: UI renders, search workers, and print workers
  already touch engines off-thread; per-engine mutexes no longer exclude
  anything once the context is shared.

**Non-Goals:**
- Touching DjVu/comic/image engines (no MuPDF use, already concurrency-clean).
- Changing the WLX API, detect string, packaging, or settings.
- A locking overhaul beyond swapping the guarded mutex (same lock discipline,
  same helpers, same call sites).

## Decisions

1. **Header-only shared context (`src/mupdfcontext.h`), never destroyed**
   - `sharedMuPdfContext()` creates one `fz_context` (plus
     `fz_register_document_handlers`) on first use via a function-static and
     hands out the pointer; `close()`/`dropDocument()`/`dropArchive()` drop
     only their own documents/archives and detach (`m_ctx`/`m_fzCtx = nullptr`).
     Never destroying removes any shutdown-order hazard between live documents
     and the context at DLL unload.
   - Rationale: header-only means zero CMake edits — the plugin target and all
     15 harness targets pick it up through the engine headers they already
     compile. Function-statics are thread-safe for the racy first `ListLoad`.
   - Alternative: a `.cpp` with explicit init/shutdown. Rejected — touches
     `CMakeLists.txt` in ~16 places for no benefit.

2. **One global MuPDF mutex replaces both per-instance mutexes**
   - Every `std::lock_guard<std::mutex> lock(m_mutex)` becomes
     `lock(sharedMuPdfMutex())`; helpers keep their "caller must hold the
     lock" contract, now against the global one. `Fb2ZipEngine` keeps its own
     mutex for `m_valid` (lock order there is always outer-own then global;
     MuPDF never calls back into it, so no cycle).
   - The `ChmResourceArchive` callbacks run inside `fz_*` calls while the
     global lock is held and call only lock-free helpers (`resolveResourcePath`
     → `entryExists`, `readEntry`) — that nesting is unchanged and safe because
     those helpers never lock.
   - Cost: all MuPDF work process-wide serializes (a search worker can briefly
     hold off a render). Correctness first; a Lister's MuPDF load is far below
     the contention where this matters.

3. **CHM stylesheet goes per-topic-document where possible**
   - With `MUPDF_HAVE_STYLE_DOCUMENT` (always on for the pinned Windows port):
     `openTopicDoc()` — the single choke point for every topic parse+layout —
     applies `chmStylesheet()` via `fz_style_document(doc, 0, css)` right after
     opening the topic doc and before `fz_layout_document`, mirroring
     `MuPdfEngine::finishOpen` ordering. The context CSS is never touched, so
     nothing can leak into other windows' documents.
   - Without it (older system libmupdf on Linux): keep `fz_set_user_css`, but
     set it inside `openTopicDoc()` immediately before parsing instead of once
     in `open()`. The global lock is held throughout, and every parse/layout
     site (MuPDF `finishOpen`, CHM `openTopicDoc`) sets its own CSS first, so
     no stale stylesheet is ever live at a parse.
   - Rationale: `chmStylesheet()` is `reflowCss() + chm.css` overrides — set
     process-wide it would visibly restyle EPUBs opened in other windows.
   - Alternative: leave context CSS as-is. Rejected — turns the concurrency
     fix into a theming regression.

4. **No behavior change on the failure path**
   - If the shared context ever fails to create (practically OOM at first
     `ListLoad`), `open()` returns false exactly like a failed private context
     did, so the host still falls through to the next plugin.

5. **CHM opens via a per-window temp copy on Windows**
   - Upstream `chmlib`'s narrow `chm_open()` calls `CreateFileA` with
     `dwShareMode=0` (verified in the vcpkg `chm_lib.c` sources: the
     `PPC_BSTR` branch shares `FILE_SHARE_READ`, the narrow branch shares
     nothing), so the first in-place open denies every later open of the same
     file. The fix is on our side: always stage a private per-window copy —
     each window gets its own file, no lock interaction at all.
   - The copy is written with raw Win32 I/O (`GetTempFileNameW` +
     `CreateFileW`/`WriteFile`, deleted via `DeleteFileW` in `dropArchive()`),
     deliberately not Qt: libchm takes exclusive ownership, so no Qt handle
     may ever exist on the staged path (a just-closed `QTemporaryFile` still
     reads as locked — `ERROR_SHARING_VIOLATION` — to the immediately
     following exclusive open in the same process, confirmed via Restart
     Manager: our own process was the holder).
   - Rationale: patching this properly means forking the port into
     `overlay-ports/` and rebuilding it everywhere for a one-flag change;
     staging is ~30 lines, needs no port rebuild, and reuses the tested
     cleanup discipline. Cost is one file copy per CHM open (CHMs are small
     documentation files; Linux keeps zero-copy in-place opens).
   - Alternative: overlay-port patch setting `FILE_SHARE_READ`. Rejected —
     heavier rollout for the same user-visible result; revisit if CHM sizes
     ever make the copy measurable.

## Risks / Trade-offs

- [Global lock contention] Search/render/print across windows serialize inside
  MuPDF. Mitigated: same granularity the per-engine mutex already imposed per
  document; Lister workloads are single-document paced.
- [Old-MuPDF CSS window] On system libmupdf < 1.28 the context CSS is still
  shared; correctness rests on the set-before-parse discipline. Mitigated: both
  setting sites are adjacent to their parse calls and covered by the same lock;
  the Windows build (primary target) never touches context CSS at all.
- [`fz_style_document` parity for CHM] Per-doc styling replaces context styling
  for topics. Mitigated: same function/arguments the MuPDF engine already uses
  for reflowables; verified by the CHM link/text harnesses plus a render
  smoke check.

## Platform-specific code

- **Shared (no `#ifdef`):** `mupdfcontext.h`, mutex swap in both engines,
  shared-context attach/detach, `openTopicDoc` styling (internally gated by
  the existing `MUPDF_HAVE_STYLE_DOCUMENT` capability probe, which the Windows
  overlay hard-codes ON).
- **Windows:** no specific work; overlay port already provides
  `MUPDF_HAVE_STYLE_DOCUMENT`, so CHM styling is per-document there.
- **Linux:** unchanged capability probing; old-lib fallback path keeps working
  through the choke-point CSS discipline.

## Migration Plan

1. Add `src/mupdfcontext.h`; repoint both engines at it (attach/detach +
   global lock + CHM per-topic styling).
2. Rebuild the plugin and harnesses; run the ICC/EPUB/CHM/link/search harnesses.
3. Re-run the two-`ListLoadW` double-open reproduction: same file twice and two
   different MuPDF files, expecting all-open with no lcms2 diagnostics.
4. Record the `viewer-rendering` delta spec; note the fix in `AGENTS.md`.
