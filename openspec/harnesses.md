# Test harnesses (developer-facing)

The project optionally builds two Windows-only harness executables with
`-DWLX_BUILD_HARNESS=ON` (Developer PowerShell / command prompt for VS 2026):

```powershell
cmake --preset windows-x64-release -DWLX_BUILD_HARNESS=ON
cmake --build --preset windows-release
```

- `harness-scroll` — validates the continuous-mode virtual canvas: scroll range,
  mid-document reachability, mixed page sizes, viewport anchoring. Generates a
  synthetic multi-page PDF itself, asserts scroll behaviour, prints PASS/FAIL.
- `harness-win` — hosts the real `ViewerWin32` in a plain window and drives real
  mouse/keyboard input via `SendInput`: drag scrolling, cursor state, scroll
  clamping, selection.

Both live in `tests/`, link the same sources as the plugin, and are not
installed. They exist to make interaction regressions reproducible without a
file manager; see the archived changes under `openspec/changes/archive/` for
the tasks each harness was built to verify.
