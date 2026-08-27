## Context

The 57 MB binary is dominated by MuPDF's embedded font set. The libmupdf vcpkg overlay builds every font under `resources/fonts/` (`urw/*.cff`, `han/*.ttc`, `droid/*.ttf`, `noto/*.otf|ttf`, `sil/*.cff`) into the static library as `resources/*.obj` (Windows) / `resources/*.c` (Linux). `source/fitz/noto.c` builds a font-family→font lookup table referencing **all** of them, so the static linker pulls every font object into the final binary: ~34 MB of it lands in the writable `.data` section. The remaining sizes come from over-declared Qt features that statically drag whole unused modules into the artifact, plus ~31 MB of unreferenced ICU DLLs left in the package. Motivation and target numbers are in proposal.md.

## Goals / Non-Goals

**Goals:**
- Shrink the Windows (`MultidocViewer.wlx64`) and Linux (`MultidocViewer.wlx64`) binaries from ~57 MB to ~23-26 MB.
- Ship MuPDF with only the Base-14 PDF font set + minimal symbol/emoji fonts, falling back to system fonts for CJK.
- Stop statically linking unused Qt modules on Windows.
- Stop shipping unreferenced ICU DLLs in the Windows package.

**Non-Goals:**
- No change to MuPDF rendering pipeline, engine interface, or WLX API (all unchanged).
- No attempt to squeeze code-size (`.text`/`.rdata`) — dominated by the font data; not worth the effort per the analysis.
- No behavior change to how text or CJK is *rendered* — only the source of glyph data changes.

## Decisions

### Decision 1: Trim the font set at the mupdf port level, via an overlay port

**Choice:** Add an `overlay-ports/libmupdf/` that keeps only the Base-14 URW `*.cff` and `CharisSIL*.cff`, and drops ALL CJK (`han/*.ttc`, `droid/*.ttf`) and Noto/emoji fonts. This is done in two coordinated places in the overlay `CMakeLists.txt`:
1. narrow the `file(GLOB fonts ...)` to only `urw/*.cff` + `sil/*.cff`, and
2. add the matching `TOFU_NOTO` + `NO_CJK` compile defines (`TOFU_NOTO` implies `TOFU_SYMBOL`+`TOFU_EMOJI`; `NO_CJK`→`TOFU_CJK`), which make `source/fitz/noto.c`/`font-table.h` skip the corresponding `_binary_*` extern references.

This coordination is required: mupdf `noto.c` builds a lookup table that `extern`-references every embedded font via `font-table.h`; the `TOFU_*` macros are the supported mechanism that removes both the table entries and (matching in the upstream Makefile) the font objects. Editing the glob alone would leave undefined `_binary_*` symbols, so the defines must mirror the glob. Keeping the change confined to the overlay port keeps it rebaseable.

**Rationale:** The existing repo already uses overlay ports (`overlay-ports/chmlib`, `djvulibre`, `icu`, `qtbase`), and `VCPKG_OVERLAY_PORTS` is already wired into every preset (`CMakePresets.json` → `vcpkg-base`). This is the smallest, most contained change: copy `C:\vcpkg\ports\libmupdf` into `overlay-ports/libmupdf`, narrow the glob + add defines, and rebuild. Per the platform decision, all CJK/emoji falls back to system fonts (DirectWrite on Windows, fontconfig on Linux), mirroring SumatraPDF.

**Alternatives considered:**
- *Runtime system-font fallback for everything (SumatraPDF's leanest form).* Adopted for CJK/emoji: keep Base14+SIL, drop all CJK. This reaches the ~22 MB target.
- *Keep Source Han Serif (`han/*.ttc`) for offline CJK.* Considered and rejected in favor of dropping all CJK to reach the size target (user decision); it would have left the binary near ~48 MB.
- *Trim the binary post-link* (strip unused COMDATs). Can't work: the linker already pulls the font objects as referenced by `noto.c`, so nothing is dead-strippable at link time.
- *Linker `/OPT:REF` + manual `noto.c` edit.* Fragile: fights the upstream table and would need re-verified at every mupdf upgrade. Overlay defines are the supported, maintainable mechanism.

### Decision 2: Drop unused Qt feature flags in `vcpkg.json`

On Windows only Qt6 `Core` + `Gui` (QImage/QString) are used. Keep `gui`, `freetype`, `harfbuzz`, `png`, `jpeg`, `zstd`, `brotli`, `doubleconversion`, `pcre2`, `thread`, `concurrent` (used by search worker). Drop Windows-irrelevant static bloat: `network`, `opengl`, `sql`, `sql-psql`, `sql-sqlite`, `widgets`, `testlib`, `dbus`, `sessionmanager`, `async-io`, `dnslookup`, `future`. If a referenced symbol requires one of these, re-add it (the build will fail fast). Because Windows uses `find_package(Qt6 COMPONENTS Core Gui)` (CMakeLists.txt:117), only Core+Gui are linked even if more features are in vcpkg.json — so this trims the *static library* bloat (the plugin still only links Core+Gui). The real Windows fix is the font; this trims the archive/install and Linux too.

**Decision 3 — CHM debug/release link (already fixed, out of scope)**

Reference only: the fix landed separately in `CMakeLists.txt` before this change. The unreferenced ICU DLLs observed in `build/release/Release/` are a vcpkg applocal post-build artifact and are **not** part of the shipped artifact (`dist/release` and the zip contain only `MultidocViewer.wlx`, `MultidocViewer.wlx64`, `pluginst.inf`), so no packaging change is needed for them.

### Platform-specific code

- **Font embedding** — both platforms benefit equally. Windows uses COFF object embedding (`.obj`), Linux uses `.c` hex arrays; the trim is in the shared port `file(GLOB ...)` so no per-platform logic.
- **Qt feature trim** — only affects the way vcpkg is configured (shared `vcpkg.json`); the actual Qt-component selection already differs (`Core`/`Gui` on Windows vs `Widgets`/`PrintSupport` on Linux) and is unchanged.
- **No ICU packaging change** — ICU DLLs are a build-tree artifact only; the shipped zip already contains just the plugin + `pluginst.inf`.

## Risks / Trade-offs

- **[Risk] CJK/emoji PDFs render with fewer glyphs after trim.** → Mitigation: CJK and emoji fall back to system fonts (DirectWrite on Windows, fontconfig on Linux), as SumatraPDF does; rare script glyphs rely on the host font stack. Users reading Latin/Base14 documents see no change.
- **[Risk] Overlay port diverges from upstream libmupdf; future upgrades need the port updated.** → Mitigation: keep the copy minimal, only the `file(GLOB ...)` + `TOFU_*` defines differ; rebase on each version bump.
- **[Risk] Dropping a Qt feature that is actually referenced fails the build.** → Mitigation: build is fast-fail; re-add the feature if the linker reports an unresolved Qt symbol. Verified: Windows links only Core+Gui+OpenGL (the qwindows platform plugin needed `opengl`, so it was kept).
- **[Trade-off] Removing the droid/noto globs loses the multilingual CSS/EPUB rendering mupdf embedded.** → Accepted: this is the whole point of the change. System fontfallback covers the common cases.

**Measured outcome:** Windows x64 57.33 MB → 22.26 MB (`.data` ~34 MB → ~1.2 MB); x86 ~53.7 MB → 18.98 MB. Both arches link cleanly with the trimmed mupdf + Qt.

## Migration Plan

1. Introduce `overlay-ports/libmupdf/` with the trimmed font glob + `TOFU_NOTO`/`NO_CJK` defines; reconfigure + rebuild x64/x86 and measure (done: 57.33→22.26 MB x64, ~53.7→18.98 MB x86). Validate rendering on representative PDF/DjVu/EPUB/CHM/CBR samples (pending, needs a TC/Double-Commander environment).
2. Trim Qt features in `vcpkg.json`; rebuild; confirm no unresolved-link misses; re-measure (done for Windows).
3. Confirm `BuildMakeSetup.bat` packaging still ships only `MultidocViewer.wlx64`, `MultidocViewer.wlx`, `pluginst.inf` (unchanged — the ICU DLLs are a build-tree artifact only).
4. Re-run `BuildMakeSetup.bat` end-to-end for both arches; confirm new zip/release sizes.

Rollback: revert the overlay font glob and `vcpkg.json` feature list; rebuild with prior versions.

## Open Questions

- ~~Is the rare exotic-script CJK coverage drop acceptable, or should Source Han Sans also be retained?~~ **Resolved:** drop **all** CJK (`han`/`droid`) to reach the size target; CJK falls back to system fonts. Confirmed during apply (measured x64 → 22.26 MB).
- ~~Keep NotoEmoji for EPUB/CSS emoji?~~ **Resolved:** do not bundle emoji fonts; emoji falls back to system fonts.