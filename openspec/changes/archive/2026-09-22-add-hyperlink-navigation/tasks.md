## 1. Shared Link Model

- [x] 1.1 Add `LinkItem` (page-space `bbox`, `destPage`, `anchorY`, `uri`) and `virtual QVector<LinkItem> pageLinks(int page)` returning an empty list to `DocumentEngine` in `src/document.h`; confirm every engine still compiles on the default
- [x] 1.2 Verify the Win32 release preset builds with the new virtual (no engine overrides yet) and the viewer is unchanged

## 2. MuPDF Link Extraction

- [x] 2.1 Implement `MuPdfEngine::pageLinks`: `fz_load_links` on the body page, `rect` → `LinkItem.bbox`, `fz_is_external_link` classification, empty list for the synthetic cover page, all under `m_mutex` with `fz_try`/`fz_catch`
- [x] 2.2 Resolve internal targets with `fz_resolve_link_dest` + `fz_page_number_from_location`, convert to 1-based public page numbering with the `m_bodyHasCover` shift, and set `anchorY` from coordinate-bearing destination types (0 otherwise)
- [x] 2.3 Declare the `pageLinks` override in `src/mupdfengine.h` and build the Win32 preset

## 3. CHM Link Resolution

- [x] 3.1 Implement `ChmEngine::pageLinks` from the open topic's MuPDF HTML document (reuse `fz_load_links`), classifying external schemes with `fz_is_external_link`
- [x] 3.2 Resolve relative hrefs against the current topic path (split off `#fragment`, percent-decode, handle `./`, `../`, leading `/`, and backslashes) and map through `normalizePath`/`pageIndexOf`; drop links that do not resolve to an HTML entry
- [x] 3.3 Resolve `#fragment` to `anchorY` by opening the target topic document and resolving the bare fragment; fall back to page top when it does not resolve
- [x] 3.4 Declare the override in `src/chmengine.h` and build the Win32 preset

## 4. Controller Link Support

- [x] 4.1 Add `linkAt(int page, const QPointF& canvasPt)` to `ViewerController`, mapping canvas to page space with `pageTransform` and testing `LinkItem.bbox`; return the link index or `-1`
- [x] 4.2 Add a per-page link cache (`QHash<int, QVector<LinkItem>>`) cleared on open/close and any relayout
- [x] 4.3 Add `followLink(page, linkIndex, scrollY)`: internal → `goToPage` plus a clamped anchor scroll (design D7); external → injected handler; unresolvable destination → no-op
- [x] 4.4 Add `setExternalLinkHandler(fn)` and route external links through it so shared code references no Qt or Win32 types
- [x] 4.5 Expose `pageLinks`/`linkAt`/`followLink` in `src/viewercontroller.h` and build the Win32 preset

## 5. Win32 Viewer Integration

- [x] 5.1 Return `IDC_HAND` from `WM_SETCURSOR` when `linkAt` hits, taking precedence over the text I-beam
- [x] 5.2 In `WM_LBUTTONDOWN`, when `GetKeyState(VK_CONTROL) < 0` and a link is under the cursor, follow it instead of starting a selection or pan
- [x] 5.3 Install the external launcher (`ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL)`) via `setExternalLinkHandler`
- [x] 5.4 Build the Win32 preset and smoke-test hover cursor, Ctrl+click navigation, and an external URL in Total/Double Commander

## 6. Linux Viewer Integration

- [x] 6.1 Set `Qt::PointingHandCursor` in the hover branch when `linkAt` hits, and restore the I-beam/arrow otherwise
- [x] 6.2 In `MouseButtonPress`, when `Qt::ControlModifier` is held and a link is under the cursor, follow it instead of starting a selection or pan
- [x] 6.3 Install the external launcher (`QDesktopServices::openUrl(QUrl(uri))`) via `setExternalLinkHandler`
- [ ] 6.4 Build `cmake --preset linux-release` and smoke-test hover cursor, Ctrl+click navigation, and an external URL

## 7. Tests and Samples

- [x] 7.1 Add `tests/harness_links.cpp`: headlessly assert internal page targets, external URIs, and anchor coordinates extracted from a linked PDF fixture
- [x] 7.2 Add CHM link coverage asserting relative-href and `#fragment` resolution against a generated topic fixture
- [x] 7.3 Add a linked sample document under `examples/` (generated through `tools/`) and a CHM link fixture for manual smoke tests
- [x] 7.4 Wire the new harnesses into `CMakeLists.txt`/the test configuration and run them

## 8. Documentation

- [x] 8.1 Update `AGENTS.md`: add a hyperlink-navigation entry to the fixed-issue history and note the `pageLinks` link model plus the Ctrl+click gesture in the architecture conventions

## 9. Modifier-gated link cursor (verification feedback)

- [x] 9.1 Show the pointing-hand cursor over a link only while Ctrl is held, and refresh it on the Ctrl key transition (Win32 `setHoverCursor`/`refreshHoverCursor` + `WM_KEYDOWN`/`WM_KEYUP`; Qt hover-branch modifier check + `KeyPress`/`KeyRelease`); update the `viewer-hyperlinks` and `viewer-interaction/text-selection` cursor requirements accordingly
