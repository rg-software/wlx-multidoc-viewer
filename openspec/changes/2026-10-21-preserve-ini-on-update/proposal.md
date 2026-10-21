# Proposal

## Why

The release archives ship a live `multidocviewer.ini` next to the plugin binary. Installing or updating via Total Commander's auto-installer extracts the whole archive, overwriting the user's customized configuration on every update — the same settings-loss problem fixed in `wlx-markdown-viewer` (`preserve-ini-on-update`).

## What Changes

- Ship the default configuration as `multidocviewer.ini.sample` instead of a live `multidocviewer.ini` on both platforms (Windows zip, Linux zip, CMake install tree).
- Keep `assets/multidocviewer.ini` as the single template source; rename at packaging time.
- No runtime code changes: an absent INI already falls back to compile-time defaults per `plugin-config`, so unlike the sister project no first-run creation is needed.

## Capabilities

### New Capabilities
- `plugin-config`: Packaging-time preservation of plugin configuration (template shipping).

### Modified Capabilities
<!-- None -->

## Impact

- Affected files: `BuildMakeSetup.bat`, `BuildMakeSetup.sh`, `.github/workflows/release.yml`, `CMakeLists.txt` (install + post-build staging).
- No runtime, API, or spec-behavior changes; existing user INI files remain untouched. Users create `multidocviewer.ini` once from the sample to customize.
