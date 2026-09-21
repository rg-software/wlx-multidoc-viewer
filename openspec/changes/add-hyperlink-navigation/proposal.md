## Why

Documents that carry hyperlinks — PDF/EPUB/XPS/MOBI cross-references and CHM topic links — render their link text but cannot be followed: clicking does nothing and external URLs are unreachable. MuPDF exposes the link APIs and the viewers already hit-test pages and jump to outline destinations, so the gap is a missing link model and click path.

## What Changes

- Add a per-page link model to `DocumentEngine` (hot-zone rectangle plus internal destination page/anchor or external URI) with a no-op default.
- Extract and resolve links in the MuPDF engine for PDF, EPUB, XPS, MOBI, FB2, and CHM topics, honoring the synthetic MOBI cover shift.
- Make CHM internal links live: resolve relative topic hrefs (including `#fragment`) against the archive page list, dropping the v1 "relative links need not be live" limitation.
- Follow links with Ctrl+click so plain drag still selects text; show a pointing-hand cursor over links, taking precedence over the text I-beam.
- Hand external links to the system handler (`ShellExecuteW` on Windows, `QDesktopServices::openUrl` on Linux).
- Add a headless link harness and a linked sample.

## Capabilities

### New Capabilities
- `viewer-hyperlinks`: Per-page link regions, hover cursor, Ctrl+click activation, internal destination navigation (page + anchor), and external URL launching.

### Modified Capabilities
- `chm-format-support`: Internal CHM topic links become navigable; the v1 "relative links need not be live" limitation is removed.
- `viewer-interaction/text-selection`: Cursor feedback and press precedence gain a hyperlink carve-out (pointing hand, Ctrl+click follows instead of selecting).

## Impact

- `src/document.h` — `LinkItem` struct plus `pageLinks()` virtual with empty default.
- `src/mupdfengine.*`, `src/chmengine.*` — link extraction, destination and anchor resolution.
- `src/viewercontroller.*` — link hit-test, `followLink`, anchor scroll, external-launcher callback.
- `src/viewer_win32.cpp`, `src/viewer.cpp` — hover cursor, Ctrl+click gating, OS URL launch.
- New `tests/harness_links.cpp` and a linked `examples/` sample. No new dependencies.
