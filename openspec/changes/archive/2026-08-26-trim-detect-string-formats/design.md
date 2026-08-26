## Context

`add-chm-support`'s smoke harness gained a dispatcher sweep that opens one probe fixture per advertised extension through the real engines. Results: PDF/EPUB/FB2/CBZ/PNG/JPG/TXT/HTML open; MD and MOBI fixtures fail; CBR/CB7 depend on `add-comic-rar-support`. Separately, the product decision is to keep internet-ish formats (HTML/TXT) out of the lister even though they technically render.

## Goals / Non-Goals

**Goals**
- Hosts only offer the plugin for extensions that render correctly.
- Stay under the WLX 260-char detect-string limit with headroom.

**Non-Goals**
- Removing engine capability: `MuPdfEngine` still renders HTML/text if a file reaches it by other means; only host registration changes.
- Touching any capability spec (nothing on record covers the removed extensions).

## Decisions

### Decision 1: Trim at registration, not in the dispatcher

`SUPPORTED_EXTENSIONS` gates which files hosts route to us; `formatdispatcher.cpp` keeps its default-to-MuPDF fallthrough untouched. Minimal diff, no behavioral surprise for direct loads.

### Decision 2: Removal list

Drop `EXT="MD"`, `EXT="HTML"`, `EXT="HTM"`, `EXT="TXT"`. Keep `MOBI` (host-verified by the user), keep `CBR`/`CB7` (made honest by `add-comic-rar-support`). Everything else already works.

## Risks / Trade-offs

- **[Risk] Users who relied on opening .txt/.html via F3 lose that.** *Mitigation:* deliberate product decision recorded here and in the README.
- **[Risk] A future MuPDF version gains Markdown/MOBI-fixture support** making this trim overly cautious for MD. *Mitigation:* re-registration is a one-line change; smoke sweep would catch it first.

## Migration Plan

Edit the macro, rebuild, confirm `ListGetDetectString` output length and contents. Rollback: restore the four tokens.

## Open Questions

None.
