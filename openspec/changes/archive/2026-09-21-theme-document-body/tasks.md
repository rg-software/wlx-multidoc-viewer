# Tasks

## 1. Palette slots

- [x] 1.1 `src/viewer_settings.h`: `documentBg`/`documentText` slots; defaults light `#FFFFFF`/`#000000`, dark `#262626`/`#E0E0E0`; read `DocumentBackground`/`DocumentText` from the active theme section.
- [x] 1.2 `assets/multidocviewer.ini` + `dist/release/multidocviewer.ini`: added both keys to both theme sections.

## 2. Shared document transform

- [x] 2.1 `src/documenttheme.h`: `isMonochrome`, `applyDuotone`, `applyToRenderedPage`, `reflowCss`; light theme is identity.
- [x] 2.2 Header-only, no CMake source change; engines include it.

## 3. Reflowable documents

- [x] 3.1 `src/mupdfengine.cpp`: `fz_style_document` (per-document, non-deprecated) with the internal stylesheet for reflowable EPUB/MOBI/HTML.
- [x] 3.2 `src/chmengine.cpp`: same stylesheet on its HTML context.
- [x] 3.3 FB2 paints its own opaque page background that user CSS cannot override (verified with `!important`), so FB2 is routed through the per-page duotone instead (text-only, so it recolors cleanly).

## 4. Fixed-layout documents

- [x] 4.1 `src/mupdfengine.cpp`: non-reflowable (and FB2) pages get the duotone when monochrome; the synthetic MOBI cover is a color image and is left alone.
- [x] 4.2 `src/djvuengine.cpp`: same per-page duotone (code path shared; not exercised end-to-end here — the reflow harness links no DjVu decoder).

## 5. Verification

- [x] 5.1 `tests/harness_document_theme.cpp` (13 checks): monochrome detection, duotone mapping + light identity, mid-gray interpolation, dark INI recolors a monochrome page, colored page untouched, reflow CSS carries the theme colors.
- [x] 5.2 `tests/harness_reflow.cpp` (manual diagnostic): EPUB dark `#262626`/light `#FFFFFF`; FB2 dark `#262626`/light `#FFFFFF`; neutral PDF dark `#262626`/light `#FFFFFF`. All PASS.
- [x] 5.3 Plugin + harnesses build on Windows; `harness-icc` still ALL PASS (light theme is identity).

## 6. Wrap-up

- [x] 6.1 `design.md` synced (FB2 → duotone; `fz_style_document`).
- [ ] 6.2 Archive the change; commit.
