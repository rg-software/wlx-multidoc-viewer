## Why

The shipped plugin is ~57 MB on both platforms while an equivalent SumatraPDF (same MuPDF engine, same DjVu, same libarchive-based zip/rar) is ~16 MB. The gap is not code but statically-bundled bloat: mupdf's vcpkg port embeds ~73 MB of Noto/Droid CJK font objects of which ~34 MB is linkable into the binary, and the project over-declares Qt features that drag in whole unused modules.

## What Changes

- **Trim MuPDF's embedded CJK + Noto font set.** Ship MuPDF with only the Base-14 PDF fonts (`urw/*.cff`) and the SIL fallback set (`sil/*.cff`), with all CJK (`han`/`droid`) and Noto script/emoji fonts removed (via `TOFU_NOTO` + `NO_CJK` in an overlay port). CJK glyphs fall back to system fonts (DirectWrite on Windows, fontconfig on Linux), like SumatraPDF. Expected binary reduction: 57 MB → ~22 MB.
- **Trim the Qt feature list in `vcpkg.json`.** On Windows only `Core`+`Gui` (QImage/QString) are used; drop unused static features (`network`, `opengl`, `sql`, `sql-psql`, `sql-sqlite`, `widgets`, `testlib`, `dbus`, `sessionmanager`, `concurrent` where unreferenced) so unused Qt module/plugin code no longer gets statically linked.
- The CHMLib release/debug-link fix is out of scope here; already fixed separately.

This is a build/packaging optimization: rendering behavior is unchanged (docs still render correctly, CJK falls back to system fonts like SumatraPDF). The font trim and Qt feature trim benefit both platforms.

## Capabilities

### New Capabilities

(none — this is a build/packaging change with no behavior change)

### Modified Capabilities

(none — the viewer rendering spec describes the render pipeline, not font-embedding policy)

This change opts out of spec deltas (`skip_specs: true`) because nothing observable changes: the viewer still renders the same pages at the same resolution; only the source of CJK glyph data and the size of the artifact change.

## Impact

- Build: `CMakeLists.txt`, `vcpkg.json`, mupdf overlay port (`overlay-ports/`).
- Dependencies: `libmupdf` (font-data selection), Qt feature selection.
- Artifact size: x64 57.3 MB → 22.3 MB, x86 ~53.7 MB → 19.0 MB (measured on Windows).
- No change to the WLX API, engine interface, or any runtime behavior contract.