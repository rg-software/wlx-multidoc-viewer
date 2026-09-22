# Technical debt

Deliberately scheduled debt for the MultidocViewer plugin. One priority axis, on the
`he9-review-contract` P scale (P0 is never recorded here — a P0 is fixed, not deferred):

- `P1` — will cause defects or block work soon.
- `P2` — the maintainability / architecture / docs / tests class, no current behavioral impact.
- `P3` — polish.

## Item template

```
### TD-### <title>
- Priority: P#
- Scope: <file:line or area>
- Category: <correctness|security|data-integrity|performance|maintainability|architecture|docs|tests>
- Source: <review reference, e.g. src/ debt scan 2026-09-22, reviewer R-#>
- Symptom: <what is wrong>
- Impact: <what happens if not fixed>
- Direction: <minimal, safe direction>
- Status: open | resolved (remove on next cleanup) | promoted (<issue/PR/change>)
```

## Items

### TD-001 Detect string advertises WEBP, which no engine can open
- Priority: P2
- Scope: `src/plugin.cpp:44-50`, `src/formatdispatcher.cpp:14-19`
- Category: correctness
- Source: src/ debt scan 2026-09-22, reviewer R-1
- Symptom: `SUPPORTED_EXTENSIONS` includes `EXT="WEBP"`, but `isRasterSuffix` excludes webp and the MuPDF fallthrough has no webp decoder, so no engine routes it.
- Impact: TC/DC offers this plugin for `.webp`; every open attempt fails (ListLoad returns NULL and the host falls through to other plugins). Also contradicts the specs (`image-engine`, `tiff-document-support` both state WEBP is out of scope).
- Direction: Either drop `EXT="WEBP"` from the detect string until the qtbase overlay gains the webp plugin (cheap, detect-string budget is fine), or treat it as the documented AGENTS.md gap and leave the advertisement in deliberately.
- Status: open

### TD-002 Per-paint CreateSolidBrush instead of the static viewerBackgroundBrush()
- Priority: P2
- Scope: `src/viewer_win32.cpp:623-628` (static brush at `:118-128`)
- Category: performance
- Source: src/ debt scan 2026-09-22, reviewer R-2
- Symptom: `onPaint()` creates and deletes a GDI brush every WM_PAINT from the same frozen `activePalette().pageBg` the process-lifetime `viewerBackgroundBrush()` already wraps.
- Impact: GDI handle create/destroy on the hottest path (selection drag, continuous scroll). No correctness impact.
- Direction: `FillRect(hdcMem, &rc, viewerBackgroundBrush())` — the static brush exists for exactly this; both sites read the frozen palette so behavior is identical.
- Status: resolved 2026-09-22 — fixed in `src/viewer_win32.cpp` (`onPaint` now uses the static brush)

### TD-003 Dead method paintSearchOverlay in the Win32 viewer
- Priority: P2
- Scope: `src/viewer_win32.h:68`, `src/viewer_win32.cpp:1422-1426`
- Category: maintainability
- Source: src/ debt scan 2026-09-22, reviewer R-4
- Symptom: `ViewerWin32::paintSearchOverlay(HDC, const RECT&, int)` is declared and defined with an all-`Q_UNUSED` body and no call sites; search highlights are drawn through `paintSelectionOverlay` (single translucent surface).
- Impact: Ghost API suggests two overlay paths exist. (The `ViewerCanvas::paintSearchOverlay` in `viewer.cpp` is a different, live method.)
- Direction: Remove the declaration and definition.
- Status: resolved 2026-09-22 — declaration (`src/viewer_win32.h`) and definition (`src/viewer_win32.cpp`) removed

### TD-004 Clipboard UTF-16 write duplicated between plugin.cpp and viewer_win32.cpp
- Priority: P2
- Scope: `src/plugin.cpp:234-246` (ListSendCommand `lc_copy`) vs `src/viewer_win32.cpp:28-45` (`setClipboardText`)
- Category: maintainability
- Source: src/ debt scan 2026-09-22, reviewer R-5 (corrected: two sites, not three — `WM_COPY` already delegates to the helper)
- Symptom: Verbatim `OpenClipboard/EmptyClipboard/GlobalAlloc/SetClipboardData(CF_UNICODETEXT)/CloseClipboard` sequence duplicated; the helper is file-local to viewer_win32.cpp so plugin.cpp cannot reuse it.
- Impact: Clipboard fixes (e.g. GlobalAlloc failure handling) must be applied twice; drift risk.
- Direction: Expose the helper from a small shared header (e.g. `win32_clipboard.h` or via `viewer_win32.h`) and call it from `ListSendCommand`.
- Status: open

### TD-005 imagefolder.h comment claims tif/tiff/webp are scanned; code excludes them
- Priority: P2
- Scope: `src/imagefolder.h:13`
- Category: docs
- Source: src/ debt scan 2026-09-22, reviewer R-7
- Symptom: Header doc says `(jpg/jpeg/png/gif/tif/tiff/bmp/webp, case-insensitive)`; `isRasterName` (`src/imagefolder.cpp:11-16`) matches only jpg/jpeg/png/gif/bmp/ico, matching the `image-folder-browsing` spec (TIFF is a document, WEBP has no decoder).
- Impact: Stale comment contradicts code and spec; a maintainer could assume the folder scan already covers TIFF/WEBP.
- Direction: Correct the comment to the actual roster (jpg/jpeg/png/gif/bmp/ico).
- Status: resolved 2026-09-22 — comment corrected in `src/imagefolder.h`

### TD-006 DLL-load-time INI file I/O from namespace-scope inline variables
- Priority: P2
- Scope: `src/viewer_settings.h:225-242` (`kSidebarInitialWidth`, `kSidebarVisibleByDefault`, `kReflowFontSize`), `src/pluginconfig.cpp:18-28`
- Category: correctness
- Source: src/ debt scan 2026-09-22, reviewer R-11 (corrected: `activePalette()` at `:173` already uses the recommended function-local-static pattern)
- Symptom: Three namespace-scope `inline` variables have dynamic initializers calling `PluginConfig::get()` (module path resolution + INI file read), which runs during DLL CRT init under the loader lock, with unspecified cross-TU ordering.
- Impact: Latent loader-lock/static-init-order hazard; currently works because consumers initialize after attach, but is fragile as the codebase grows.
- Direction: Convert the three to Meyer's-singleton accessors (`inline int kReflowFontSize() { static const int v = ...; return v; }`) so the file I/O happens lazily at first use, off the loader lock.
- Status: open

### TD-007 Raster-suffix predicate defined three times independently
- Priority: P2
- Scope: `src/formatdispatcher.cpp:16-19`, `src/imagefolder.cpp:11-16`, `src/viewercontroller.cpp:1401-1406`
- Category: maintainability
- Source: src/ debt scan 2026-09-22, reviewer R-12
- Symptom: `isRasterSuffix` / `isRasterName` / `isRasterPath` all define the same set (jpg/jpeg/png/gif/bmp/ico) independently; the set is pinned by the `image-folder-browsing` spec.
- Impact: Adding a raster format (e.g. WEBP once a decoder exists) requires touching three files plus the spec; one site can be missed.
- Direction: One shared predicate (e.g. `isNativeRasterSuffix()` in a platform-agnostic header) used by all three sites.
- Status: open

### TD-008 Raw OEM virtual-key hex codes for zoom/reset/sidebar hotkeys
- Priority: P3
- Scope: `src/viewer_win32.cpp:1004-1022`
- Category: maintainability
- Source: src/ debt scan 2026-09-22, reviewer R-3
- Symptom: `0xBB`/`0x6B`/`0xBD`/`0x6D` lack the named-constant comments the neighbouring lines (`0x60` VK_NUMPAD0, `0xBF` VK_OEM_2, `0x7B` VK_F12) already carry; they are `VK_OEM_PLUS`/`VK_ADD`/`VK_OEM_MINUS`/`VK_SUBTRACT`.
- Impact: Readability only; note the OEM VKs remain physical-key-position based on non-US layouts — the rename does not change that behavior.
- Direction: Replace hex with the `VK_*` names (zero behavior change); optionally document the physical-key layout nuance.
- Status: resolved 2026-09-22 — renamed to `VK_OEM_PLUS`/`VK_ADD`/`VK_OEM_MINUS`/`VK_SUBTRACT`/`VK_NUMPAD0`/`VK_OEM_2`/`VK_F12` in `src/viewer_win32.cpp` (values byte-identical)

### TD-009 Unnecessary const_cast on a const-callable pageText
- Priority: P3
- Scope: `src/viewercontroller.cpp:1015`
- Category: correctness
- Source: src/ debt scan 2026-09-22, reviewer R-6
- Symptom: `wordAtCanvas` (const) uses `const_cast<ViewerController*>(this)->pageText(page)`, but `pageText` is already const (`viewercontroller.h:171`) over a `mutable` cache (`:376`) — four sibling const methods call it directly.
- Impact: UB-adjacent smell and inconsistency with the other const callers (`:1042`, `:1061`, `:1100`, `:1152`).
- Direction: Drop the `const_cast`; call `pageText(page)` directly.
- Status: resolved 2026-09-22 — `const_cast` removed in `src/viewercontroller.cpp` (`pageText` is const)

### TD-010 Manual per-pixel RGB→BGR swap in QImageToBitmap
- Priority: P3
- Scope: `src/viewer_win32.cpp:103-113`
- Category: performance
- Source: src/ debt scan 2026-09-22, reviewer R-10
- Symptom: Hand-rolled per-pixel R/B byte swap for every DIB row; Qt provides `QImage::rgbSwapped()` with optimized paths.
- Impact: Small CPU cost per newly rendered page; no correctness impact.
- Direction: Replace the loop with `img.convertToFormat(QImage::Format_RGB888).rgbSwapped()` (keeping the existing stride handling).
- Status: resolved 2026-09-22 — conversion switched to `QImage::Format_BGR888` in `src/viewer_win32.cpp` (`QImageToBitmap`), per-pixel loop replaced by row memcpy; output bytes verified identical semantics

### TD-011 harness-scroll G36 is timing-flaky (~50% on identical binaries)
- Priority: P2
- Scope: `tests/harness_scroll.cpp:788` (G36 "Single geometry identical to the original")
- Category: tests
- Source: verification pass of the 2026-09-22 src/ debt scan (trivial-fix bundle)
- Symptom: G36 fails nondeterministically — measured 3/7 failures with the scan's bundle build and 3/5 on clean HEAD (same binary flips PASS/FAIL between runs), so it is independent of any code change.
- Impact: A known-flaky check trains maintainers to ignore red builds and can mask real geometry regressions in the same assert.
- Direction: Investigate the G-series pump timing (`pump(40)`) around the Single -> ... -> Single presentation cycle — likely an async relayout or refit landing outside the pump window — and make the assertion wait for a stable layout or capture geometry deterministically.
- Status: open