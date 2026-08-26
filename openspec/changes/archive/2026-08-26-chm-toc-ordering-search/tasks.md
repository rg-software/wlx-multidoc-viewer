## 1. Engine internals refactor (no behavior change)

- [x] 1.1 Extract a shared `openHtmlPage(page)` helper in `ChmEngine` that reads the entry, applies codepage handling, opens the per-page `fz_document`, and returns doc+page; rewire `renderPageLocked` through it
- [x] 1.2 Rebuild and re-run the CHM smoke test to confirm identical rendering behavior before continuing

## 2. Reading-order pagination

- [x] 2.1 Resolve the default topic: `/#SYSTEM` type-2 entry first, else `/#WINDOWS` row `+0x68` offset via `/#STRINGS`; empty when neither resolves
- [x] 2.2 Locate the `.hhc` path: `/#SYSTEM` type-1 entry first, else `/#WINDOWS` `+0x60` via `/#STRINGS`
- [x] 2.3 Implement the page-order builder: `[home] + [.hhc topic locals in order] + [remaining archive entries]`, de-duplicated by normalized path, replacing the plain archive-order list set at open
- [x] 2.4 Verify with smoke test: home topic is page 1 on a system fixture; every HTML entry still maps to exactly one page

## 3. Nested outline from `.hhc`

- [x] 3.1 Implement a tolerant case-insensitive `.hhc` scanner: track `<ul>`/`</ul>` nesting depth, collect `Name`/`Local` param pairs from `<object type="text/sitemap">` blocks, skip comments/scripts
- [x] 3.2 Decode basic HTML entities (`&amp;`, `&lt;`, `&gt;`, `&quot;`, `&#NNN;`) in Name/Local values
- [x] 3.3 Map scanner output to nested `QVector<OutlineItem>` with destinations resolved against the reordered page list (unresolved → page 1)
- [x] 3.4 Wire fallback chain into `outline()`: `.hhc` parse → flat `/#WINDOWS` rows → empty
- [x] 3.5 Smoke-test: nesting shape matches fixture TOC; unresolved locals land on page 1

## 4. Text layer + search

- [x] 4.1 Implement `pageText`/`extractText` over the shared helper, porting word assembly (whitespace splitting, line grouping) from `MuPdfEngine::pageText`
- [x] 4.2 Implement `hasSelectableText` override and flip `supportsSearch()` to true
- [x] 4.3 Implement `searchText` with normalized page-space rects, honoring match-case, including the cross-line space-separator handling from `MuPdfEngine::searchText`
- [x] 4.4 Smoke-test: search finds terms across several pages of a system fixture with correct case behavior; selection words non-empty

## 5. Codepage table completion

- [x] 5.1 Replace the LCID subset with the full SumatraPDF-style `LcidToCodepage` mapping (1252 fallback retained)

## 6. Build verification

- [x] 6.1 `cmake --build --preset windows-release` green; plugin binary produced
- [ ] 6.2 Linux preset build on the Linux dev box (`chmengine.cpp` compiles against Qt6 Widgets path)

## 7. Host walkthrough

- [x] 7.1 Open a real `.chm` in Total/Double Commander: viewer lands on the home topic; next/prev/first/last/jump follow TOC order
- [x] 7.2 Outline panel shows the nested `.hhc` tree; activating items navigates to the right pages
- [x] 7.3 Find UI searches across pages with highlights; match-case toggle behaves; text selection + copy works
- [x] 7.4 Record any failures and fold them into follow-up work rather than this change
- [x] 7.5 Walkthrough finding: forward Esc from the TOC tree/panel to the viewer window so it behaves exactly like main-window Esc (Win32 tree subclass + focus restore; Qt event-filter parity)
- [x] 7.6 Walkthrough finding: programmatic TOC selection must not re-activate entries (TVN_SELCHANGED loop yanked scroll to page tops once sections span pages), and the active row must be scrolled into view (replaced bogus `TreeView_SelectSetFirstVisible(count)` call)
- [x] 7.7 Walkthrough finding: destination-less TOC containers (`<param name="Local">` absent, e.g. "Background Information" in Frotz.chm) masqueraded as page-1 candidates in reading-position sync; `OutlineItem.resolved` flag added (document.h), presenter skips unresolved entries when matching the current page; path matching also strips `#fragment` anchors
