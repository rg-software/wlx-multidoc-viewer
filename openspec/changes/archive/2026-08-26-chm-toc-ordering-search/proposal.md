## Why

First host testing of `add-chm-support` surfaced three usability gaps that all trace to documented v1 scope decisions: the viewer opens on whichever HTML entry happens to sit first in archive order (not the help's home topic), the outline shows a single item (one row per `/#WINDOWS` window entry) instead of the real table of contents, and find/selection are disabled because the engine reports no text layer.

## What Changes

- Reading-order pagination: page 1 becomes the CHM's default topic (home), followed by topics in `.hhc` table-of-contents order, then any remaining `.htm`/`.html` entries in archive order; duplicates are removed so every entry stays reachable and the page set is stable.
- Nested outline parsed from the `.hhc` file (tolerant HTML scanner, no new dependency), preserving `<ul>` nesting as `OutlineItem` children. Falls back to the v1 flat `/#WINDOWS` outline when `.hhc` is missing or unparseable, and to archive order when no ordering source exists.
- Text layer via MuPDF's structured-text pipeline over each rendered HTML page: implement `extractText`, `pageText`, `hasSelectableText`, `supportsSearch`, and `searchText`, enabling the viewer's find UI (with match-case), text selection, and copy for CHM.
- Replace the compact LCID subset with the full `LcidToCodepage` mapping (SumatraPDF table).

## Capabilities

### New Capabilities
<!-- None for this change. -->

### Modified Capabilities
- `chm-format-support`: page enumeration changes from archive order to reading order (home topic first, then `.hhc` topic order); the outline requirement changes from `/#WINDOWS`-only flat rows to `.hhc`-based nested TOC with `/#WINDOWS` fallback; adds a requirement for a selectable/searchable text layer.

## Impact

- `src/chmengine.{h,cpp}` — all of the work: shared per-page HTML-document helper, `.hhc` scanner, page reordering at open, stext-based text/search implementations, LCID table.
- No build-system or dependency changes (no Gumbo link target needed).
- Viewer/toolbar/sidebar/print surfaces unchanged: they already consume `DocumentEngine`'s text/search APIs generically; CHM simply stops reporting "no capability".
- Search worker thread (`searchcontroller`) will now call into `ChmEngine::searchText` from its worker thread — covered by the engine's existing mutex discipline.
