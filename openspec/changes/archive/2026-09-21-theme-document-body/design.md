## Context

`viewer-theme` themed the chrome and explicitly excluded document content. In dark mode a white PDF page or a reflowable EPUB/CHM body therefore glows against the dark surround. MuPDF (linked) provides the primitives needed: `fz_is_document_reflowable`, `fz_set_user_css` (reflowable), and `fz_tint_pixmap` (luminance duotone). CHM renders through the same HTML pipeline; DjVu is a separate raster engine. Raster images and comics must stay untouched.

## Goals / Non-Goals

**Goals:**
- Document bodies follow the active theme, with no extra switch.
- Reflowable documents render against the theme background with theme text.
- Fixed-layout pages that are background-neutral (monochrome) are recolored with the theme colors; pages with real color are preserved.
- The behavior is expressed as two INI palette slots, like the rest of the theme.

**Non-Goals:**
- No per-pixel "smart" recolor (per-page granularity only, by decision).
- No new INI switches — light theme is a natural no-op.
- No recoloring of images or comics.
- No change to print output.

## Decisions

### D1 — Two document slots, light theme is a no-op

Add `documentBg`/`documentText` to `Palette` and to both INI theme sections. Defaults: light `#FFFFFF`/`#000000`; dark `#262626`/`#E0E0E0` (the dark document background is deliberately a touch lighter than the chrome `pageBg #1E1E1E` so pages remain distinguishable from the surround). Because the light pair is black-on-white, every transform below reduces to identity in light mode — no switch needed.

### D2 — Reflowable documents: internal user stylesheet from the slots

For a reflowable document (`fz_is_document_reflowable` on the MuPDF engine; always true for the CHM HTML pipeline) apply an internal stylesheet with `fz_style_document` (the non-deprecated per-document API; `fz_set_user_css` is deprecated in 1.28) before layout, built from the slots:

```css
html, body { background-color: #<documentBg>; }
body { color: #<documentText>; }
```

Setting the background on `html`/`body` and the text color on `body` means element-level document backgrounds/colors still win where a document sets them, while neutral bodies take the theme. Verified: EPUB and the CHM/HTML pipeline take the dark background. **FB2 is the exception** — MuPDF's FB2 handler paints an opaque page background the stylesheet cannot override (tested with `!important`), so FB2 is routed through the per-page duotone (D3) instead; FB2 is text-only, so the duotone recolors it cleanly.

### D3 — Fixed-layout documents: per-page neutrality check + duotone

After a page is rendered to a `QImage`, a shared helper classifies it: compute each pixel's chroma (`max(r,g,b) - min(r,g,b)`) and treat the page as monochrome when the fraction of pixels above a small chroma threshold is below a small fraction (a page with a real color logo/photo counts as colored). A monochrome page is remapped by luminance between `documentText` and `documentBg`:

```
out = text + (bg - text) * luminance/255     (per channel)
```

Applied to the MuPDF engine's non-reflowable output (PDF/XPS/TIFF) and to the DjVu engine's output. In light mode this is identity. `fz_tint_pixmap` is the MuPDF-native equivalent; a small QImage loop keeps the logic shared with DjVu and is unit-testable.

### D4 — Images and comics excluded

`imageengine`/`comicengine` are not touched; their pages are photographic/graphic by nature.

### D5 — Thresholds

Chroma threshold and colored-fraction cutoff are compile-time constants in the shared helper, tuned so anti-aliased gray text and scans stay monochrome while any genuinely colored region marks the page colored.

## Risks / Trade-offs

- [Reflowable CSS precedence] → A document that sets `html` background (rare) is overridden; one that sets `body`/element backgrounds or colors is preserved. Documented as the intended policy.
- [Per-page false positives] → A mostly-text page with a small colored logo stays light (all-or-nothing per page). Accepted by decision; per-pixel recolor is a possible follow-up.
- [Per-page false negatives] → A near-monochrome photo could be duotoned. The chroma threshold guards this; tuned conservatively.
- [Render cost] → One extra pass over each rendered page for the chroma check (fixed-layout only). Bounded by page pixel count; acceptable.
- [MOBI cover] → The synthetic cover page is a color image, so it is left untouched by the neutrality check.

## Migration Plan

- INI gains two optional keys per theme section; absent keys fall back to the built-in defaults. No breakage, no switch to flip.
- Rollback: revert the change; document bodies return to their own colors.

## Open Questions

None — per-page granularity, no switches, and format rules are settled.
