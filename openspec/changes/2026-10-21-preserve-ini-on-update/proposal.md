# Proposal

## Why

The release archives ship a live `multidocviewer.ini` next to the plugin binary. Installing or updating via Total Commander's auto-installer extracts the whole archive, overwriting the user's customized configuration on every update — the same settings-loss problem fixed in `wlx-markdown-viewer` (`preserve-ini-on-update`).

## What Changes

- Stop shipping `multidocviewer.ini` itself in the release archives and install tree; ship `multidocviewer.ini.sample` instead (Windows zip, Linux zip, CMake install).
- Read the configuration in layers: the shipped `.sample` is the fallback base, the user's `multidocviewer.ini` overlays it key by key, and hardcoded read-site defaults apply only when a key is in neither file. Audited: every active sample value already equals its hardcoded default, so no-INI behavior is unchanged.
- Add `harness-pluginconfig` covering sample-only, user overlay, user-only, and neither-file scenarios.

## Capabilities

### New Capabilities
- `plugin-config`: Packaging-time preservation of plugin configuration (template shipping).

### Modified Capabilities
<!-- None -->

## Impact

- Affected files: `BuildMakeSetup.bat`, `BuildMakeSetup.sh`, `.github/workflows/release.yml`, `CMakeLists.txt` (install + post-build staging).
- No runtime, API, or spec-behavior changes; existing user INI files remain untouched. Users create `multidocviewer.ini` once from the sample to customize.
