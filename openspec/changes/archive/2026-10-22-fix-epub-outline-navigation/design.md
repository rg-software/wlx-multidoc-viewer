# Design

## Context

See `proposal.md`. The outline model is platform-agnostic: `DocumentEngine::outline()`
returns a tree of `OutlineItem { title, pageNo, anchorY, resolved, children }`,
`SidebarPresenter` flattens it into pre-order rows, and activating a row calls
`ViewerController::goToPage(pageNo)`.

`pageNo` is the *public* 1-based page number (it includes the synthetic MOBI
cover page, which shifts the body pages by one). `anchorY` is the normalized
(0..1) vertical position of the destination inside that page. `resolved` is
false for entries without a resolvable destination; the sidebar skips them
when tracking the reading position, and activation falls back to page 1.

`MuPdfEngine::outline()` reads `fz_outline::page` directly. That works for
PDF, XPS and HTML/MOBI, because their MuPDF loaders resolve the destination
before handing the node over. The EPUB loader does not: `epub_parse_ncx_imp`
and `epub_parse_nav_imp` fill `fz_outline::uri` with the raw target (for
example `OEBPS/ch1.xhtml#sec-1-2`) and leave `fz_outline::page` at
`fz_make_location(-1, -1)`.

The same URIs are resolved correctly by `pageLinks()`, which calls
`fz_resolve_link_dest()` and maps the location through
`fz_page_number_from_location()`.

## Goals / Non-Goals

**Goals:**
- An EPUB table-of-contents entry navigates to the page it points at.
- A fragment target lands on the fragment, not at the top of its page.
- Entries with an unresolvable destination are visible but do not masquerade
  as page-1 entries.
- No behavior change for the formats whose destinations MuPDF already
  resolves.

**Non-Goals:**
- Patching MuPDF's EPUB loader in the `libmupdf` overlay port (see Decisions).
- Giving PDF/XPS/HTML outline entries an in-page anchor: their destinations
  live in PDF user space (y-up) and would need a different conversion, which
  is out of scope here.
- Changing the sidebar or the controller: they already navigate correctly
  once the engine reports a real page.

## Decisions

1. **Resolve the URI in the engine, not in MuPDF.** The `libmupdf` overlay
   port could patch `epub-doc.c` to resolve the destination while building the
   outline, but that means carrying a patch across every port bump and
   rebuilding the dependency (VS developer environment plus `VCPKG_ROOT`).
   `fz_resolve_link_dest()` is a public API that the engine already uses for
   in-page links, so the engine-side fix is a few lines and needs no
   dependency change.

2. **Keep MuPDF's pre-resolved destination as the fast path.** When
   `fz_outline::page` is valid the engine keeps using it unchanged, so PDF,
   XPS and HTML/MOBI behave exactly as before. Only a node whose destination
   is unresolved falls back to the URI.

3. **Resolve the URI with the same call `pageLinks()` uses.**
   `fz_resolve_link_dest()` yields a `fz_link_dest`; for EPUB the destination
   type is `FZ_LINK_DEST_XYZ` and `dest.loc` is a `(chapter, page-in-chapter)`
   location. `fz_page_number_from_location()` turns that into the global
   0-based body page, which is then shifted to the public page number with
   the MOBI cover shift. A table-of-contents entry and the equivalent link
   inside the page therefore always land on the same spot — the harness
   asserts exactly that.

4. **Populate `anchorY` for the resolved entry.** `OutlineItem::anchorY`
   already exists and the CHM engine populates it, but the MuPDF engine never
   did. For EPUB the destination is an XYZ destination whose `y` is a
   top-down offset inside the reflowed A5 slice, so `dest.y / pageHeight`
   (clamped to 0..1) is the same normalization `pageLinks()` uses. Reflowable
   formats paginate into slices, so a fragment target is typically mid-page
   and a page-top-only jump would be visibly wrong.

5. **Mark unresolvable entries `resolved = false`.** The `resolved` flag
   already exists for entries without a destination (the sidebar's own
   Favorites header uses it). An EPUB entry whose target is not in the spine
   must not be reported as a page-1 entry. Activation of such an entry falls
   back to page 1, which is the documented behavior for destination-less
   entries.

6. **Resolve inside the existing shared lock.** `outline()` already holds the
   process-wide MuPDF lock, and the resolution runs under it. It forces the
   EPUB chapter layout, which `pageCount()` has already forced by the time
   the sidebar is populated, so it adds no new blocking work.

## Risks / Trade-offs

- **Layout cost.** `fz_resolve_link_dest()` lays out the chapters a table of
  contents points at. For EPUB those chapters are laid out anyway while the
  page count is computed, so the outline adds no measurable cost.
- **Longjmp across C++ frames.** MuPDF's `fz_try`/`fz_catch` is
  setjmp/longjmp based. The resolution therefore happens in a helper that
  keeps the values it needs across the `fz_try` block in locals marked with
  `fz_var()`, and the catch block overwrites them before they are read.
- **A different MuPDF version could resolve the destination itself.** Then
  the fast path applies and the behavior is unchanged.

## Platform-Specific Code

None. The change is confined to the shared `MuPdfEngine` (plus its header).
Both the Win32 viewer on Windows and the Qt viewer on Linux consume the same
outline model, so both benefit without platform-specific code. The new
harness is a Windows-only target, matching the existing harness suite
(`WLX_BUILD_HARNESS` is gated on `WIN32`).

## Migration Plan

None. The change is internal to the engine; no configuration, no stored
state, no user-visible setting changes.
