# Test harnesses (developer-facing)

The project optionally builds a set of Windows-only harness executables with
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
- `harness-epub-toc` — validates that outline entries resolve to a real
  destination page. Covers the EPUB table-of-contents parsers MuPDF supports
  (the EPUB3 navigation document and the EPUB2 NCX), the in-page anchor a
  fragment target carries, the consistency between a table-of-contents entry
  and the in-page link to the same target, and the sidebar activation path
  through the real `SidebarPresenter` and `ViewerController`. Takes the
  `examples` directory as its argument:
  `harness-epub-toc examples`.

All of them live in `tests/`, link the same sources as the plugin, and are not
installed. They exist to make interaction regressions reproducible without a
file manager; see the archived changes under `openspec/changes/archive/` for
the tasks each harness was built to verify.
