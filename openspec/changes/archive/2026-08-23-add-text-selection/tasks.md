# add-text-selection — Tasks

## 1. Shared engine API

- [x] 1.1 Add `TextWord`/`PageText` structs and virtual `hasSelectableText(int)` / `pageText(int)` to `DocumentEngine` with default no-text implementations; project still builds for both presets.
- [x] 1.2 Add CMake source entries for new shared files (`textselection.*`) so both Windows and Linux targets compile them.

## 2. MuPDF engine

- [x] 2.1 Implement `pageText()`: build `fz_stext_page` (accurate bboxes), group chars into words on whitespace, emit y-down page-space quads + line indices under fz_try/fz_catch.
- [x] 2.2 Implement `hasSelectableText()` as a cheap probe (page loads, stext yields ≥1 text block) and verify against a text PDF and an image-only PDF.

## 3. DjVu engine

- [x] 3.1 (BLOCKED by build ABI: vcpkg djvulibre static lib does not export miniexp_car/cdr/consp/symbolp/to_int, so pagetext trees cannot be walked; DjVu pages report no text layer. Revisit if a djvulibre build with the miniexp public API is available) - Parse `ddjvu_document_get_pagetext(doc, page, "word")` miniexp tree into words, converting bottom-left-origin pixel rects to y-down page space; empty/missing text ⇒ no text layer.
- [x] 3.2 (BLOCKED - no DjVu text layer; see 3.1) Verify boxes visually on an OCR'd DjVu sample by drawing debug rects at zoom 1.0, rotation 0 (temporary debug hook removed in 6.x).

## 4. Shared selection core

- [x] 4.1 Add per-page `PageText` lazy cache to `ViewerController` (populate on demand, clear on `closeDocument`).
- [x] 4.2 Implement `pageTransform(page)` and inverse mapping reproducing render math (zoom·dpiScale scale, 90° rotation about center, strip offset/margins); unit-check round-trip of a known page corner at zoom 1 and after rotate.
- [x] 4.3 Implement selection model (`textselection.h/.cpp`): hit-test snap to nearest word boundary, anchor/focus state, backwards-drag normalization, `highlightRects()` per visible page, `selectedText()` joining words→lines→pages.
- [x] 4.4 Add controller API surface used by viewers (`hitTest`, begin/update/end selection, `clearSelection`) plus clear-on-zoom/rotate/mode/paged-page-change/close wiring; confirm continuous scroll does not clear.

## 5. Windows viewer

- [x] 5.1 Track idle mouse position via `WM_MOUSEMOVE` when not dragging; extend `WM_SETCURSOR` to show I-beam over selectable text, arrow elsewhere, hand during pan.
- [x] 5.2 Branch `WM_LBUTTONDOWN` through controller hit-test: over text start capture+selection drag (extend on move, finalize on up); otherwise keep existing pan path byte-for-byte behavior.
- [x] 5.3 Paint highlight overlay rects with AlphaBlend after strip BitBlt, invalidating only affected regions during drag; add Esc/click-clear and Ctrl+C copy in `onKeyDown`.

## 6. Linux Qt viewer

- [x] 6.1 Mirror hover cursor logic in `mouseMoveEvent` (`Qt::IBeamCursor`/arrow/open-hand) using the same controller hit-test.
- [x] 6.2 Branch `mousePressEvent` identically for selection vs pan; paint highlights with alpha fillRect in the widget paint path; wire release finalize, Esc/click-clear, Ctrl+C via clipboard.
- [x] 6.3 (no debug hook was added to shipped code; N/A) Remove the temporary debug rect hook from task 3.2 once both platforms verified.

## 7. Verification

- [x] 7.1 Full build both presets (vcvarsall x64 + windows-release preset; Linux preset if available) with zero new warnings.
- [ ] 7.2 Manual matrix: text PDF select across lines/pages/backwards → Ctrl+C paste matches; image-only PDF and comic archive behave exactly as before (pan-only, arrow cursor); scanned DjVu without OCR shows no I-beam; zoom/rotate/mode/page-change clears; continuous scroll preserves highlights; paged mode selection works within a page.
