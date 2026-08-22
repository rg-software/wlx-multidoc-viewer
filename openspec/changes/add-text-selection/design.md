# add-text-selection — Design

## Context

The viewer paints one strip bitmap per document (`renderVisiblePages` → HBITMAP / QImage) and knows nothing about text geometry. `DocumentEngine::extractText` returns plain text only: MuPDF already builds an `fz_stext_page` with accurate bboxes and throws the geometry away (src/mupdfengine.cpp:120); the DjVu stub returns empty (src/djvuengine.cpp:168). Left-drag pans in continuous mode (src/viewer_win32.cpp:376) and `WM_MOUSEMOVE` is only handled while dragging; `WM_SETCURSOR` currently sets a hand cursor during pan only (src/viewer_win32.cpp:257). Strip layout stacks pages at a fixed stride (`pageStride()`) with margins and centering applied in `onPaint`. See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**
- A geometry-aware, platform-agnostic text API on `DocumentEngine` implemented by both engines.
- One shared selection model (hit-test, range state, transforms, extracted string, highlight rects) in shared code, consumed thinly by both viewers.
- I-beam hover feedback, drag-select with overlay highlight, `Ctrl+C` copy on Windows and Linux.

**Non-Goals:**
- Sub-word selection precision — endpoints snap to word boundaries.
- Find-in-page, link clicking, annotation, accessibility bridges, selection toolbar UI.
- Fixing pre-existing strip gaps (mixed page sizes, stride cap); selection inherits their exact limitations rather than adding new ones.

## Decisions

### D1: Word-granularity items as the common currency

Extend `document.h`:

```cpp
struct TextWord { QString text; QRectF bbox; int lineIndex; }; // page space, y-down
struct PageText  { bool hasText = false; QVector<TextWord> words; };
// DocumentEngine additions:
virtual bool hasSelectableText(int page);   // cheap probe
virtual PageText pageText(int page);        // full geometry-aware extraction
```

- **MuPDF**: one `fz_new_stext_page_from_page` per page (`FZ_STEXT_ACCURATE_BBOXES`, as today), group chars into words on Unicode whitespace via `fz_stext_char::quad` unions. Alternative considered: raw char quads like SumatraPDF's glyph arrays — rejected because DjVu's miniexp parsing at `'char'` granularity is disproportionately expensive and word-level covers highlight + copy; the spec's "nearest text item boundary" wording permits this.
- **DjVu**: parse `ddjvu_document_get_pagetext(doc, page, "word")` — nested `(page (column (para (line (word x0 y0 x1 y1 "text")))))` sexprs. DjVu rects use bottom-left origin in page pixels: flip Y to top-left/y-down here so both engines emit identical coordinate conventions. Empty tree ⇒ `hasSelectableText == false`.
- Default implementations return "no text" so any future engine (CHM, images) needs no changes to stay correct.

### D2: Lazy per-page cache in ViewerController, not engines

`ViewerController` holds `QHash<int, PageText> m_textCache`, populated on first hover/hit-test/paint need, cleared in `closeDocument()`. Rationale: keeps engines stateless, keeps extraction off the render hot path, single invalidation point. Documents are static while open, so no other invalidation is needed. Cache size is bounded by document length (same accepted bound as the existing strip cap).

### D3: One transform builder for hit-test and painting

`ViewerController::pageTransform(int page) const -> QTransform` reproduces exactly what `renderPage`/`onPaint` do: scale by `zoom * dpiScale`, rotate about the rendered-page center in 90° steps, translate by strip offset `(page-1)*stride` plus margin/centering. Its inverse maps pointer client coords → page-space points; its forward form maps word bboxes → device rects for painting. MuPDF stext quads (identity ctm ⇒ unrotated page space, same space as `pageDimensions`) match this pipeline. Because selection uses the same stride math as rendering, known strip gaps degrade identically instead of diverging.

### D4: Selection model in shared code

New `textselection.h/.cpp` used by `ViewerController`:

- State: anchor + focus as snapped `(page, wordIndex, after/before-edge)` positions.
- Hit-test: nearest word boundary by center-distance rule (word-level analogue of SumatraPDF's `FindClosestGlyph`).
- Range resolution normalizes backwards drags; output APIs:
  - `highlightRects(page) -> QVector<QRect>` device-space, computed per paint from current scroll — recomputed every frame, never cached in device space, which is what makes scrolling transparently keep selections.
  - `selectedText() -> QString`: words joined by spaces within a line, `\n` between `lineIndex` groups and pages.
  - Clipboard write itself lives in the viewers (Qt `QClipboard` wraps native clipboard on both platforms), not in the model.

### D5: Input split — press point decides gesture (Sumatra-style)

Platform-neutral decision helper in the controller: `HitKind hitTest(clientPoint)` returning `None | Text | Elsewhere`.

- **Windows**: extend `WM_SETCURSOR` (not dragging): query controller hit-test using last hover position tracked by newly-added idle-path `WM_MOUSEMOVE` handling (today it early-outs when not dragging) → `IDC_IBEAM` over text, arrow elsewhere, hand during pan. `WM_LBUTTONDOWN` branches: over text ⇒ capture + selection drag; otherwise existing pan path untouched. Highlights drawn after the strip `BitBlt` with `AlphaBlend` (~50% blue), no re-render involved.
- **Linux Qt**: mirror in `viewer.cpp` — `setCursor(Qt::IBeamCursor/OpenHandCursor/ArrowCursor)` from the same hit-test in `mouseMoveEvent`; branch `mousePressEvent` identically; paint highlights in the existing widget paint; copy via `QApplication::clipboard()`.

### D6: Copy and clear hooks

- Win32: check `'C'` + `GetKeyState(VK_CONTROL)` in the existing `onKeyDown`; Qt: `keyPressEvent` (Ctrl+C). No-op when selection empty — clipboard untouched (spec).
- Clear on `Esc`, on click that neither starts nor extends a range, zoom/rotation/mode toggle/page change in paged mode/close — all routed through one `ViewerController::clearSelection()` called from the few places those events already funnel through (`onControllerChanged` callers). Continuous-mode scroll deliberately does *not* call it.

## Platform-specific code

| Concern | Windows | Linux |
|---|---|---|
| Hover tracking | idle `WM_MOUSEMOVE` + `WM_SETCURSOR` | `mouseMoveEvent` without buttons |
| Cursor ids | `IDC_IBEAM` / `IDC_ARROW` / `IDC_HAND` | `Qt::IBeamCursor` / `ArrowCursor` / `OpenHandCursor` |
| Highlight painting | GDI `AlphaBlend` over blitted strip | `QPainter::fillRect` with alpha color |
| Clipboard | `QClipboard` (Gui) | `QApplication::clipboard()` |
| Copy key | `GetKeyState(VK_CONTROL)` in `WM_KEYDOWN` | key event modifiers |

Everything else — structs, extraction, caching, transforms, selection math, text assembly — is shared, no `#ifdef` outside viewer files.

## Risks / Trade-offs

- [Word-only granularity feels coarse on long words] → Acceptable v1; char-level can be added per-engine later without changing the model's shape.
- [MuPDF stext rebuild cost per page on first hover] → One-shot per page, cached; typical pages are milliseconds; probe `hasSelectableText` avoids extraction on pure-image documents where possible.
- [`fz_stext_quad` vs our transform mismatch under rotation] → Both derive from the identical scale+rotate-about-center composition; verified against `renderPage` math in tasks before wiring UI.
- [DjVu sexpr coordinate convention mistakes] → Flip Y once at parse; task includes a visual check against a known OCR'd DjVu sample.
- [Highlight flicker during drag] → Overlay-only painting; invalidate affected rect, strip bitmap untouched.
- [Hover hit-test on every mouse move] → Linear scan of cached words with coarse bbox early-out; thousands of words max per page.

## Migration Plan

Additive feature; no data or API breaks. Rollback = revert commit; new struct fields don't alter existing engine ABI consumers.

## Open Questions

None blocking. Selection color choice (fixed light blue vs system highlight color) is a cosmetic follow-up.
