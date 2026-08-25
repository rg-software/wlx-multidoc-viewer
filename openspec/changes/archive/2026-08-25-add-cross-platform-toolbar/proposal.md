## Why

Both viewers expose all controls as hidden hotkeys only (arrows, V, Shift+V); there is no visible UI for navigation, zoom, rotation, or mode switching, and no way at all to print or find text. Total Commander provides none of these controls for lister plugins, so the plugin needs its own on-window toolbar on both Windows (Win32) and Linux (Qt).

## What Changes

- Add a cross-platform toolbar docked at the top of the viewer window, identical in function on Win32 and Qt, implemented as thin per-platform wrappers (Win32 common controls / Qt widgets) behind one shared interface that drives `ViewerController`.
- Controls: print; current-page edit box with `/ N` total-pages label; previous/next page; continuous-mode toggle; fit-mode button cycling Manual → Fit-to-page → Fit-to-width with its icon reflecting the active mode; rotate left/right; zoom out/in; a sidebar toggle where applicable.
- Add a copy button reserved for copying selected text, shipped as a permanently disabled placeholder: text selection is owned by a separate future change, and this limitation is documented in the spec.
- Add a toggleable left sidebar listing the document's embedded outline/TOC when available; activating an entry navigates to its page; documents without an outline get no sidebar.
- Add text-find box, next/previous match buttons, and a match-case toggle.
- New engine API for text search returning page + rectangle positions per match (MuPDF and DjVuLibre backends); both viewers draw highlight overlays for matches.
- Printing defers to the system print dialog (PrintDlgEx on Windows, native dialog on Linux): the chosen printer and options drive spooling of rendered pages.
- Toolbar state (current page, zoom, mode icons) stays synchronized with `ViewerController`; existing keyboard shortcuts remain unchanged. This consciously supersedes the earlier "no toolbar" convention.

## Capabilities

### New Capabilities
- `viewer-toolbar`: All on-window chrome — the control strip and the outline sidebar — covering layout, control set, state reflection, and cross-platform behavioral parity
- `viewer-text-search`: Whole-document text search with match highlighting, next/previous iteration, and case sensitivity option
- `viewer-printing`: Printing document pages via the system print dialog

### Modified Capabilities

_(none — no archived main specs exist yet; prior changes are unarchived deltas)_

## Impact

- New: `src/toolbar*` (shared control interface + per-platform backends), `src/sidebar*` (shared outline-sidebar presenter + per-platform panels), `src/print*` (print pipeline)
- Modified: `src/viewer_win32.*`, `src/viewer.*` (host the toolbar), `src/viewercontroller.*` (search/highlight state, print hooks), `src/document.h` (search API), `src/mupdfengine.*`, `src/djvuengine.*` (search implementations)
- Build: possible new link dependency `Qt6::PrintSupport` on Linux only; Windows uses winspool/gdi32 (already linked)
