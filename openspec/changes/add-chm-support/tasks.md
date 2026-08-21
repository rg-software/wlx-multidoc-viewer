## 1. Build / dependency wiring

- [ ] 1.1 Add `"chmlib"` to `dependencies` in `vcpkg.json`
- [ ] 1.2 Add `find_package(chmlib CONFIG REQUIRED)` and `chmlib::chmlib` to the link line in `CMakeLists.txt`
- [ ] 1.3 Add `src/chmengine.cpp` to the library sources in `CMakeLists.txt`
- [ ] 1.4 Update `src/plugin.cpp`: extend `SUPPORTED_EXTENSIONS` to include `EXT="CHM"` and re-verify `static_assert(sizeof(SUPPORTED_EXTENSIONS) <= 260)`

## 2. CHM archive access via libchm

- [ ] 2.1 In `src/chmengine.{h,cpp}` declare `ChmEngine : public DocumentEngine` with the standard interface plus private libchm state (`chm_ctx*`, entry list, codepage)
- [ ] 2.2 Implement `ChmEngine::open(path)` to read the file into memory and call `chm_open(ctx, buffer, size)`
- [ ] 2.3 Implement `ChmEngine::close()` to call `chm_ctx_free` and drop member state
- [ ] 2.4 Enumerate entries with `chm_get_entries`; build a `QVector<QString>` `m_htmlPages` filtered to paths ending in `.htm` / `.html` in archive order; populate `m_pageInfo[]` with the path on each index
- [ ] 2.5 Implement `pageCount()` to return `m_htmlPages.size()`
- [ ] 2.6 Implement `pageDimensions(page)` based on MuPDF's measurement of the rendered HTML (call `MuPdfEngine::renderPage` at zoom=1, dpi=1 and read the resulting `QImage::size()`)

## 3. HTML rendering through MuPDF

- [ ] 3.1 In `ChmEngine::renderPage(page, zoom, dpiScale, rotation)`: read the entry with `chm_read_entry`, then build a temporary `QByteArray` of the HTML bytes
- [ ] 3.2 Open a per-render `fz_document` via `fz_open_document_with_buffer(ctx, "html", buffer, len)` wrapped in `fz_try` / `fz_catch`; drop it on return
- [ ] 3.3 Delegate rasterization to a private `MuPdfEngine` instance (or to shared MuPDF helpers); honor zoom, dpiScale, and rotation parameters exactly as `MuPdfEngine::renderPage` does today
- [ ] 3.4 Return the resulting `QImage`; on failure return an empty `QImage`

## 4. v1 outline (no Gumbo)

- [ ] 4.1 Implement `ChmToc::Parse` to read `/#WINDOWS` and `/#STRINGS` from the archive
- [ ] 4.2 Parse the 8-byte header (count, entrySize) and iterate window rows at the documented offsets (+0x14 = title, +0x60 = toc, +0x64 = index, +0x68 = home); allow entrySize ≥ 188
- [ ] 4.3 Resolve each path offset against `/#STRINGS` (null-terminated, stop on out-of-bounds)
- [ ] 4.4 Build `QVector<OutlineItem>` where each item's `pageNo` is the index of the matching HTML entry in `m_htmlPages`, or 1 if not found
- [ ] 4.5 Implement `ChmEngine::outline()` to return the parsed vector; return empty on malformed input

## 5. Codepage detection (v1 limited)

- [ ] 5.1 Read `/#SYSTEM`, parse the binary entries (type=3 = title, type=4 = codepage LCID, type=9 = creator) at the documented offsets
- [ ] 5.2 Map the LCID to a Windows codepage (table from SumatraPDF's `LcidToCodepage`, or accept 1252 for unknown)
- [ ] 5.3 Store the codepage on the engine; when constructing the per-page render, prepend `<meta charset>` when codepage is non-UTF-8 (best-effort)

## 6. Format dispatch + detect string

- [ ] 6.1 Update `src/formatdispatcher.cpp` so the `.chm` lower-case suffix returns `std::make_unique<ChmEngine>()`
- [ ] 6.2 Confirm the new `SUPPORTED_EXTENSIONS` macro in `src/plugin.cpp` fits in 260 chars (target: 255 chars)
- [ ] 6.3 Confirm `ListGetDetectString` still emits the new value at runtime

## 7. Spec walkthrough

- [ ] 7.1 Walk every scenario under `specs/chm-format-support/spec.md` against a real `.chm` fixture (open a file from Total Commander / Double Commander); record passes and failures
- [ ] 7.2 Verify navigation (next / prev / first / last / jump) by lister keyboard inputs
- [ ] 7.3 Verify the v1 outline appears in the outline panel (if a host with outline UI is available)
- [ ] 7.4 Open a follow-up change for any scenario that fails or for any gap that requires Gumbo / codepage remap

## 8. Build verification

- [ ] 8.1 Run `cmake --preset windows-x64-release && cmake --build --preset windows-release` and confirm `wlx-multidoc-viewer.dll` is produced
- [ ] 8.2 Run the equivalent Linux preset (`cmake --preset linux-x64-release && cmake --build --preset linux-release`) on the Linux dev box

## 9. Future work (tracked separately)

- [ ] 9.1 Open a follow-up change `chm-html-toc-and-codepage` covering Gumbo-based `.hhc` / `.hhk` parsing, full `LcidToCodepage` table, and `/#IDX` support
