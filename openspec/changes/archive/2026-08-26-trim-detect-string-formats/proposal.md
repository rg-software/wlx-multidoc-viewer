## Why

The detect string registers extensions the plugin cannot render, so hosts offer the lister for files that then fail to open: MD and MOBI were verified broken by automated smoke tests over generated fixtures; HTML/HTM/TXT render but are deliberately dropped per product decision (no "internet formats" in the lister).

## What Changes

- Remove `EXT="MD"`, `EXT="HTML"`, `EXT="HTM"`, `EXT="TXT"` from `SUPPORTED_EXTENSIONS` in `src/plugin.cpp`.
- Keep everything else: MOBI is host-verified working; CBR/CB7 are repaired by `add-comic-rar-support`; PDF/EPUB/FB2/XPS/images/CHM/DjVu stay.
- Detect string shrinks from 236 to ~192 visible chars — comfortably inside the 260-char WLX limit.

## Capabilities

### New Capabilities
<!-- None. -->

### Modified Capabilities
<!-- None. No main-spec requirement documents the removed entries: the only
     format-detection requirement on record is CHM-specific
     (chm-format-support), which stays untouched. -->

This change carries no spec deltas (`skip_specs`): it removes host-facing
registrations for formats that no capability spec documents. The README
already stopped advertising them.
