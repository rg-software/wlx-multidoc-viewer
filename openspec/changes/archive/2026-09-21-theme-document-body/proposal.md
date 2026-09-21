## Why

The `viewer-theme` change themed the chrome but deliberately left document content untouched, so in dark mode a white PDF page (or a reflowable EPUB/CHM body) glows white against a dark surround. Users expect the document body to follow the theme where it is background-neutral, while documents that paint their own background must keep it.

## What Changes

- **Two new palette slots** (`DocumentBackground`, `DocumentText`) in the `[Theme:light]`/`[Theme:dark]` INI sections, exposed the same way as the other chrome colors. Light = white paper/black text (a no-op); dark = dark paper/light text.
- **Reflowable documents** (EPUB, MOBI, FB2, HTML, CHM) are styled with an internal user stylesheet built from those slots (`fz_set_user_css`), so the body renders against the theme background with theme text. The stylesheet is an implementation detail — users only edit INI colors.
- **Fixed-layout documents** (PDF, XPS, TIFF, DjVu) are recolored per rendered page: if the page is effectively monochrome it is remapped by luminance between `DocumentText` and `DocumentBackground` (`fz_tint_pixmap` / an equivalent duotone pass); if the page carries real color it is left untouched. This is the practical form of "follow the INI colors unless color is explicitly coded".
- **Images and comics are never recolored.**
- **No new switches**: the behavior follows the active theme (light theme is a natural no-op).
- This MODIFIES the `viewer-theme` requirement that documents are unaffected by the theme.

## Capabilities

### Modified Capabilities
- `viewer-theme`: the "documents unaffected by theme" requirement becomes "document bodies are themed per format/neutrality"; a new requirement defines the reflowable/fixed-layout rules. Adds the two document slots to the palette.
- `plugin-config`: the `Theme palette sections` requirement gains `DocumentBackground` and `DocumentText`.

## Impact

- `src/viewer_settings.h`: `documentBg`/`documentText` slots + light/dark defaults + INI keys.
- `src/mupdfengine.cpp`: user CSS for reflowable docs; per-page neutrality check + duotone for fixed-layout.
- `src/chmengine.cpp`: user CSS (renders as HTML).
- `src/djvuengine.cpp`: per-page neutrality check + duotone.
- `assets/multidocviewer.ini` (+ `dist/release`): the two new keys in both theme sections.
- Specs: deltas on `openspec/specs/viewer-theme/spec.md` and `openspec/specs/plugin-config/spec.md`.
