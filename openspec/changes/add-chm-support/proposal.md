## Why

CHM (Microsoft Compiled HTML Help) files are widely used for offline software manuals but are not in the lister's supported formats. Adding CHM brings manual-like documentation into the same viewer surface as PDF/DjVu/EPUB and roughly matches what SumatraPDF does for CHM browsing. MuPDF cannot natively open CHM, so we add a dedicated engine backed by libchm (the canonical C library, available as a vcpkg port called `chmlib`).

## What Changes

- Add `chmlib` to `vcpkg.json` dependencies; add `chmlib::chmlib` to the link line in `CMakeLists.txt`.
- New `ChmEngine` in `src/chmengine.{h,cpp}` implementing `DocumentEngine`: opens a CHM archive via libchm, enumerates `.htm`/`.html` entries as "pages", renders each page by handing the entry bytes to MuPDF (HTML→raster via `fz_open_document_with_buffer`).
- Update `src/formatdispatcher.cpp` to route `.chm` to `ChmEngine`.
- Update `SUPPORTED_EXTENSIONS` in `src/plugin.cpp` (still under the 260-char WLX limit) to include `EXT="CHM"`.
- New `ChmToc` v1: derive a flat outline from `/#WINDOWS` offsets in the CHM binary. Document Gumbo-based HTML TOC parsing as future work.
- Codepage handling: read `/#SYSTEM` codepage, default 1252. Other codepages are best-effort in v1.

## Capabilities

### New Capabilities
- `chm-format-support`: lister detects and opens `.chm` files, the user can navigate pages (next/prev/first/last/jump), a basic outline from `/#WINDOWS` is exposed via the engine's `outline()` API, and each page renders as a bitmap through MuPDF.

### Modified Capabilities
<!-- None for this change. -->

## Impact

- `src/formatdispatcher.cpp` — add `.chm` case.
- `src/plugin.cpp` — extend `SUPPORTED_EXTENSIONS` macro. **Must stay under 260 chars; current is 243, +12 chars = 255, fits.**
- `src/document.h` — no change to the `DocumentEngine` interface (CHM fits the existing shape).
- `src/chmengine.{h,cpp}` — new files; ~400 LoC.
- `vcpkg.json` — add `"chmlib"` (no version pin; vcpkg's baseline picks 0.40).
- `CMakeLists.txt` — add `chmlib::chmlib` to `target_link_libraries`.
- `overlay-ports/` — none needed; the upstream vcpkg port works with our `x64-windows-static-md` triplet.
- Out of scope (future work): Gumbo-based HTML TOC parsing, codepages other than 1252/CP_ACP, index (`/#idx`) parsing, frames/redirects.
