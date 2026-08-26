## Context

`add-chm-support` shipped `ChmEngine` with archive-order pagination, a flat `/#WINDOWS` outline, and no text layer (see its design Decisions 3–6). Host testing showed all three decisions degrade usability for real CHMs: the first enumerated entry is rarely the home topic, a typical CHM declares one help window so the outline collapses to a single row, and the viewer disables find/selection when an engine reports no text. See proposal.md — Why.

Relevant existing machinery: each page render already builds a fresh MuPDF HTML document from the entry bytes (`fz_open_document_with_buffer`); `MuPdfEngine` demonstrates the stext word-assembly and normalized search-rect patterns this change reuses; `searchcontroller` drives whole-document search through `DocumentEngine::searchText` on a worker thread.

## Goals / Non-Goals

**Goals**
- Deterministic reading order for CHM pages with the home topic as page 1.
- A real, nested table of contents in the sidebar.
- Text selection, copy, and find-with-match-case for CHM.
- No new third-party dependencies.

**Non-Goals**
- `.hhk` index parsing (no index UI in the lister).
- Live relative-link navigation between pages (still v1 behavior).
- Codepage remapping beyond the LCID→codepage table (bytes are transcoded via the platform API where available, unchanged otherwise).

## Decisions

### Decision 1: Hand-rolled tolerant `.hhc` scanner instead of Gumbo

`.hhc` files are small HTML documents made of nested `<ul>` lists whose items are `<object type="text/sitemap">` blocks carrying `<param name="Name"|"Local" value="...">`. A case-insensitive tag scanner that tracks `<ul>`/`</ul>` nesting depth and collects Name/Local pairs covers the overwhelming majority of real-world files without adding a link-time dependency (Gumbo is only available transitively inside MuPDF, and wiring it as a direct dependency buys robustness we do not need at this file complexity).

- *Alternatives considered:* `find_package(unofficial-gumbo)` + DOM walk (mirrors SumatraPDF) — rejected: new direct dependency, ABI/link surface, for marginal gain; MuPDF's HTML parser — rejected: it produces layout boxes, not a queryable DOM.
- *Effect:* `parseHhc()` returns `QVector<OutlineItem>` with children; malformed input yields an empty vector, which triggers fallback.

### Decision 2: Page list composed as [home] + [.hhc topics] + [remaining archive entries]

At open time the engine resolves the default topic (`/#SYSTEM` type 2, else `/#WINDOWS` `+0x68`), parses `.hhc` (path from `/#SYSTEM` type 1, else `/#WINDOWS` `+0x60`, resolved against `/#STRINGS`), then builds the page order by concatenating home, TOC-topic locals, and the untouched archive-order list — de-duplicated by normalized path (lowercase, leading `/` stripped). Every HTML entry stays reachable, the count is stable, and outline destinations resolve against the same list.

- *Alternatives considered:* strictly TOC-only pages — rejected: entries not referenced by the TOC would become unreachable; sort alphabetically — rejected: matches no mental model.
- *Effect:* "Open shows page 1" now lands on the home topic; navigation order follows the book's TOC.

### Decision 3: Shared per-page HTML-document helper feeding render, text, and search

Refactor the current `renderPageLocked` body so opening the per-page `fz_document` lives in one helper that returns doc+page under the engine mutex. `renderPageLocked`, `pageText`, `extractText`, and `searchText` all consume it; word assembly and normalized search rects are ported from `MuPdfEngine::pageText`/`searchText` (including the multi-line space-separator trick for cross-line needles). `supportsSearch()` returns true.

- *Alternatives considered:* delegating to a private `MuPdfEngine` instance — rejected again (it opens by path, not buffer; see add-chm-support apply notes).
- *Effect:* one code path for geometry conventions; search worker thread safety covered by the existing single engine mutex (same discipline as `MuPdfEngine`).

### Decision 4: Full `LcidToCodepage` table

Replace the 20-entry subset with SumatraPDF's complete LCID→ANSI-codepage mapping (~70 entries), keeping 1252 as the fallback for unknown LCIDs.

- *Effect:* non-Western CHMs get correct transcode targets on Windows; behavior elsewhere unchanged (Latin-1 approximation).

## Risks / Trade-offs

- **[Risk] Hand-rolled HTML scanning breaks on exotic `.hhc` markup** (attributes in unusual order, uppercase tags, comments). *Mitigation:* scanner is order-insensitive on attributes, case-insensitive on tags, skips comments/scripts; any doubt → empty result → documented fallbacks keep the viewer functional.
- **[Risk] `pageText`/`searchText` re-layout the HTML per call**, so searching a large document costs one layout per visited page. *Mitigation:* CHM pages are small; `searchcontroller` already renders progressively per page and supports atomic cancel.
- **[Risk] Reordering changes what existing users saw in v1 builds.** *Mitigation:* ordering only applies within a freshly opened document; no persisted state depends on page indices.

## Migration Plan

1. Refactor engine internals behind the shared helper (no behavior change).
2. Add `.hhc` parsing + page reordering; extend the smoke test with order/nesting checks against system CHM fixtures.
3. Add text-layer/search implementations; smoke-test find scenarios headlessly.
4. Swap in the full LCID table.
5. Manual verification in Total/Double Commander (navigation, outline panel, find UI, selection/copy).

Rollback: revert the single engine pair; dispatcher/detect string untouched by this change.

## Open Questions

None.
