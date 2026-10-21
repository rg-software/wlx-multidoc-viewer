# Design

## Context

See `proposal.md`. `PluginConfig::get()` reads `multidocviewer.ini` from the plugin's own directory (DLL on Windows, `.so` on Linux) and is already absence-tolerant: a missing file yields an empty structure and every setting falls back to its built-in default. The INI is never written at runtime. Today every packaging path copies `assets/multidocviewer.ini` under its live name into the install image, so TC's auto-installer clobbers user edits on update.

## Goals / Non-Goals

**Goals:**
- Updating the plugin never overwrites an existing `multidocviewer.ini`.
- Fresh installs still ship discoverable defaults (as a template).

**Non-Goals:**
- Runtime creation of `multidocviewer.ini` from the template (unlike the sister project, whose plugin warned when the file was missing — ours silently uses defaults, and the plugin directory is usually read-only).
- Migrating or versioning INI schemas.
- Changing `chm.css` shipping (separate user-editable file, out of scope).

## Decisions

1. **Rename at packaging time, keep the source stable**
   - `assets/multidocviewer.ini` stays the single template source; every packaging step copies it to `multidocviewer.ini.sample`.
   - Rationale: mirrors the sister change; source paths and the many archive references to the template stay valid.
   - Alternative: rename in source. Rejected — wider diff, same result.

2. **No first-run copy**
   - Absent INI already means defaults (specified, harness-covered). Adding a copy would need plugin-dir writability handling for zero behavioral gain.
   - Alternative: sister-style auto-create. Rejected per above.

3. **Build-tree staging follows the same rule**
   - The CMake post-build copy next to the binary also stages the `.sample`, so a rebuild never clobbers a dev's build-tree `multidocviewer.ini` either. Harnesses write their own temp INI and are unaffected.

## Risks / Trade-offs

- [Existing installs] Already-clobbered settings cannot be restored; only future updates are protected.
- [Discoverability] New users must copy the sample once to customize. Mitigated: the sample keeps all comments; absence still means sane defaults.
- [Template drift] None beyond today: the sample derives from the same source file.

## Platform-specific code

- **Windows**: `BuildMakeSetup.bat` copy line + header comment; `release.yml` Windows package step.
- **Linux**: `BuildMakeSetup.sh` copy line + header comment; `release.yml` Linux package step.
- **Shared (CMake, both platforms)**: `install(FILES ...)` destination entry and the post-build `copy_if_different` staging.

## Migration Plan

1. Update the five copy sites to emit `multidocviewer.ini.sample`.
2. Rebuild; confirm the build tree stages the sample and no live INI.
3. Verify archives/install image contain the sample and no live INI; run the INI harnesses.
