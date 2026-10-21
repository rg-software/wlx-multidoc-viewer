# Tasks

## 1. Shared packaging (CMake)

- [x] 1.1 In `CMakeLists.txt`, change the `install(FILES ...)` entry and the post-build `copy_if_different` staging to emit `multidocviewer.ini.sample` instead of `multidocviewer.ini`; update the adjacent comment; verify with a normal build that the sample appears next to the binary and no live INI is staged
- [x] 1.2 Run `harness-startup`, `harness-theme`, and `harness-findbox` and confirm they still pass (they manage their own temp INI files)

## 2. Windows packaging

- [x] 2.1 In `BuildMakeSetup.bat`, copy `assets\multidocviewer.ini` to `%OUT%\multidocviewer.ini.sample` and update the header comment; verify by running the packaging copy block and checking `dist\release` holds the sample and no live INI
- [x] 2.2 In `.github/workflows/release.yml` (Windows package step), copy to `dist\release\multidocviewer.ini.sample`; verify the YAML still parses

## 3. Linux packaging

- [x] 3.1 In `BuildMakeSetup.sh`, copy to `$OUT/multidocviewer.ini.sample` and update the header comment; verify by inspection against the `.bat` twin plus a dry run of the copy semantics in a scratch dir (no local bash; `bash -n`/CI for syntax)
- [x] 3.2 In `.github/workflows/release.yml` (Linux package step), copy to `dist/linux-release/multidocviewer.ini.sample` (covered by the same YAML parse check as 2.2)

## 4. Verification

- [x] 4.1 Confirm no packaging path references a live `multidocviewer.ini` destination anymore (`grep` over `BuildMakeSetup.*`, `release.yml`, `CMakeLists.txt`)
- [x] 4.2 Confirm runtime behavior is unchanged: absent INI yields defaults (existing `harness-startup` default scenario), present INI is honored

## 5. Layered sample fallback (runtime)

- [x] 5.1 Audit every active sample value against its hardcoded read-site fallback and confirm equality (all `[Viewer]` keys, all 26 palette slots)
- [x] 5.2 Implement the base-plus-overlay merge in `PluginConfig::get()`; hardcoded fallbacks stay as the last layer
- [x] 5.3 Add `harness-pluginconfig` (sample-only / user overlay / user-only / neither-file scenarios, spawned children) and wire it in `CMakeLists.txt`; confirm it passes
- [x] 5.4 Re-run `harness-startup`, `harness-theme`, `harness-findbox` against the layered read and confirm they still pass
