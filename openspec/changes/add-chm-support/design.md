## Context

The project currently has two engines wired into the format dispatcher (`src/formatdispatcher.cpp`): `MuPdfEngine` (PDF, XPS, EPUB, images, HTML, MD, TXT) and `DjVuEngine` (`.djvu`, `.djv`). Both implement the same `DocumentEngine` shape (`src/document.h`): `open`, `close`, `isOpen`, `pageCount`, `renderPage(page, zoom, dpiScale, rotation)`, `extractText`, `metadata`, `outline`, `pageDimensions`.

CHM is not in `SUPPORTED_EXTENSIONS` (`src/plugin.cpp`) and MuPDF cannot open CHM natively. The CHM format is documented at <https://www.nongnu.org/chmspec/latest/>; it is an archive of HTML entries plus a small set of binary control files (`/#WINDOWS`, `/#STRINGS`, `/#SYSTEM`, `/#IVB`).

The reference implementation we consulted is SumatraPDF, whose `src/ChmFile.{h,cpp}` and `src/ChmModel.{h,cpp}` use libchm + manual binary parsing + Gumbo for HTML TOC + WebView2 for rendering. We cannot lift that approach wholesale: libchm parses the archive but renders HTML via WebView2, and our viewer returns bitmaps via `renderPage`. We pick libchm for archive access and hand HTML bytes to MuPDF for rasterization.

## Goals / Non-Goals

**Goals**
- Add `chmlib` to the build via vcpkg.
- New `ChmEngine` that satisfies the existing `DocumentEngine` interface so the viewer surface stays unchanged.
- Page enumeration by archive-order of `.htm` / `.html` entries; rendering via MuPDF HTML pipeline.
- Outline from `/#WINDOWS` + `/#STRINGS` byte-offset table; HTML-side outline parsing deferred.
- Codepage 1252 (and `CP_ACP`) supported; other codepages rendered as-is with best-effort.
- Fit under the 260-char WLX detect-string limit.

**Non-Goals**
- HTML-side TOC (`.hhc`) parsing via Gumbo — documented as future work.
- Index (`/#IDX`) parsing.
- Codepage remapping for non-1252 / non-ACP chars (deferred).
- Live relative-link navigation between CHM pages (document URL → page index) — v1 opens the home page, links not active in the host.
- JS / frames / scripts.

## Decisions

### Decision 1: Render HTML via MuPDF, not WebEngine

ChmEngine holds a separate `MuPdfEngine` instance internally. For a CHM page, it reads the HTML bytes from the archive into memory and calls `MuPdfEngine::renderPage()` after handing the buffer to a per-render `fz_document` opened with `fz_open_document_with_buffer`. MuPDF's HTML pipeline is limited but covers the bulk of CHM content produced by HTML Help Workshop (vanilla HTML, inline CSS, basic images).

- *Alternatives considered:*
  - **WebEngine / CEF / Qt WebEngine**: pixel-perfect, but adds ~150 MB of binaries, conflicts with the static-link static-md triplet, and requires an offscreen render path that Qt for WLX (Win32-only static QtCore+QtGui) does not include.
  - **Custom CSS-aware text renderer**: doable but loses images, tables, scripts.
  - **Hand off HTML to a sister executable (e.g., wkhtmltopdf)**: brittle external dep.
- *Effect:* One internal `MuPdfEngine` per `ChmEngine`; HTML→raster goes through MuPDF and inherits its existing DPI handling, zoom math, and rotation pipeline (so the CHM viewer gets 90° rotation, DPI scaling, etc. for free).

### Decision 2: Use libchm (jedrea.com) via vcpkg

`vcpkg.json` gets `"chmlib"` (no version pin — the existing baseline chooses 0.40). The upstream `ports/chmlib/portfile.cmake` uses `vcpkg_check_linkage(ONLY_STATIC_LIBRARY)`, which fits our `x64-windows-static-md` triplet. We do not need a custom overlay port — the upstream works.

- *Alternatives considered:* writing our own CHM parser by hand from the binary spec (SumatraPDF owns one internally). Rejected: libchm exists, is mature, and has a maintained vcpkg port.
- *Effect:* New vcpkg dependency; new link target `chmlib::chmlib`. The download URL is `http://www.jedrea.com/chmlib/chmlib-0.40.zip` — owned by one maintainer; the vcpkg portfile pins a SHA512, so the build is reproducible.

### Decision 3: Page-order = archive enumeration order

`chm_get_entries` returns entries in file order; we keep that order and filter to entries whose path ends in `.htm` / `.html`. This matches SumatraPDF's behaviour and avoids depending on `/#WINDOWS` for pagination, which would be brittle on malformed CHMs.

- *Effect:* Page count and ordering are stable; outline destinations are resolved against this list at load time.

### Decision 4: v1 outline from `/#WINDOWS` bytes only

We read the documented offset table at `/#WINDOWS` (and `/#STRINGS`) and emit one outline entry per window row. We do not walk `.hhc` HTML, do not use Gumbo, do not parse `/#IDX`. This is the simplest outline that roughly matches what SumatraPDF surfaces in the first-level tree.

- *Reference:* SumatraPDF `ChmFile::ParseWindowsData` reads the same file at the same offsets (8 bytes header = entries × size, then `entrySize` of 188 bytes typical; columns at +0x14 = name offset, +0x60 = toc path offset, +0x64 = index path offset, +0x68 = home path offset into `/#STRINGS`).
- *Effect:* Outline is fast to compute and survives CHMs whose HTML TOC is broken or absent.

### Decision 5: One CHM page ↔ one MuPDF document

We open a fresh `fz_document` per page render and dispose it when the page is done. CHM pages are small HTML strings; per-page open is cheap and avoids stale state when the user navigates.

- *Effect:* memory bounded; render latency pays one open per page (acceptable for an interactive viewer).

### Decision 6: Codepage handling is v1 best-effort

We read `/#SYSTEM` for the declared codepage. If it is 1252 we tell MuPDF the page is Latin-1; otherwise we attempt `CP_ACP` (Windows ANSI) and fall back to plain bytes. Other codepages produce mojibake, which the spec acknowledges. Adding `<chmlib>` properly with a future Gumbo-based TOC gives us a place to plug full codepage remap (matches SumatraPDF's `LcidToCodepage` table) without breaking v1 builds.

- *Effect:* Shippable baseline; explicit path to broader codepage coverage.

### Decision 7: WLX detect string still fits in 260 chars

Current `SUPPORTED_EXTENSIONS` is 243 chars. Adding `EXT="CHM"` is 11 chars (plus a `|` separator) = 255 chars. Within the limit. No breaking change.

- *Effect:* Total Commander and Double Commander offer the lister for `.chm` files without reducing existing coverage.

## Risks / Trade-offs

- **[Risk] MuPDF HTML rendering is less faithful than WebView2.** *Mitigation:* Document the gap; mark a follow-up "Gumbo + Qt WebEngine" change if users need it. v1 is good enough for the bulk of `.chm` files produced by HTML Help Workshop.
- **[Risk] Codepage 1252-only support shows mojibake for Cyrillic / CJK CHMs.** *Mitigation:* Best-effort in v1; codepage remap is in the non-goals list, queued.
- **[Risk] v1 outline only contains first-level entries from `/#WINDOWS`.** *Mitigation:* Documented; Gumbo-based HTML TOC parsing is the obvious future change.
- **[Risk] Linkage grows: libchm is a small but new static dep.** *Mitigation:* vcpkg-static-md compatible; reproducible (SHA512 in portfile). Total binary size delta is small.
- **[Risk] `/#WINDOWS` byte offsets differ across CHM producers.** *Mitigation:* We tolerate malformed tables (verify entry-size sanity, fall back to empty outline).
- **[Risk] CHM page-relative links break in v1.** *Mitigation:* Spec scenario explicitly states relative links need not be live in v1.

## Migration Plan

1. Implement `ChmEngine` and `/#WINDOWS` parser.
2. Wire dispatch + detect string.
3. Build on Windows; manual-test against a representative `.chm` (the project will need a test fixture — see task 4.3).
4. Roll forward: if any user reports link/TOC gaps, open a follow-up change for Gumbo + codepage remap. Do **not** backport into this change — the spec lock is preserved.

## Open Questions

None at design time. Tasks 4.3 (build + manual smoke test) may reveal specific MuPDF HTML edge cases; if so, open a small follow-up change rather than expand this one.

## Future Work (deferred from this change)

- Gumbo-based HTML TOC walker (mirrors SumatraPDF `WalkChmUl`).
- `/#IDX` index parser (mirrors SumatraPDF `WalkChmIndexItem`).
- Full codepage remap (`LcidToCodepage` + `FixChmTocEntitiesTemp` style entity-fix).
- Live relative-link navigation between CHM pages.
- Optional chapter/name lookup via `/#IVB` topic-id resolution.
