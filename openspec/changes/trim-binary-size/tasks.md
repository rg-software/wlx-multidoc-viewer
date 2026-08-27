## 1. Shared: mupdf font-set trim via overlay port

- [x] 1.1 Add `overlay-ports/libmupdf/` by copying the current `C:\vcpkg\ports\libmupdf` port (portfile.cmake, CMakeLists.txt, unofficial-libmupdf-config.cmake.in, vcpkg.json)
- [x] 1.2 In the overlay's `CMakeLists.txt`, narrow the `file(GLOB fonts ...)` to keep only `urw/*.cff` and `sil/*.cff`, and drop `han`/`droid`/`noto` (with matching `TOFU_NOTO` + `NO_CJK` compile defines); user decision: drop all CJK, keep Base14+SIL
- [x] 1.3 Bump/re-fetch the overlay so `VCPKG_OVERLAY_PORTS` selects it (reconfigure x64 + x86; confirm vcpkg builds `libmupdf` from `overlay-ports/libmupdf`)
- [x] 1.4 Rebuild Windows x64 Release and record the `MultidocViewer.wlx64` size (**22.26 MB vs 57.33 MB**) — verify with dumpbin that `.data` no longer contains the Noto/Droid font markers (`.data` 34 MB → 1.2 MB)

## 2. Shared: Qt feature trim in vcpkg.json

- [x] 2.1 In `vcpkg.json`, reduce the `qtbase` feature list to Windows-referenced ones (`gui`, `freetype`, `harfbuzz`, `png`, `jpeg`, `zstd`, `brotli`, `doubleconversion`, `pcre2`, `thread`, `concurrent`, plus `opengl` which the qwindows platform plugin requires) and drop `network`, `sql`, `sql-psql`, `sql-sqlite`, `widgets`, `testlib`, `dbus`, `sessionmanager`, `async-io`, `dnslookup`, `future`, `openssl`
- [x] 2.2 Rebuild Windows x64; confirm no unresolved Qt symbols (only Qt6Core/Qt6Gui/Qt6OpenGL are linked; build succeeds cleanly)
- [x] 2.3 Note/verify Linux still selects `Widgets`+`PrintSupport` via CMakeLists (unchanged) and that Linux still configures+builds — vcpkg.json is Windows-manifest-only; Linux uses system Qt (`find_package` on CMakeLists.txt:146)

## 3. Verification (both platforms)

- [x] 3.1 Smoke-render representative samples across formats (PDF with Latin, PDF with CJK, DjVu, EPUB, CHM, CBR) on Windows and confirm no text regression vs pre-trim build — confirmed working in Total/Double Commander (user verified)
- [ ] 3.2 Re-run `cmake --preset linux-release && cmake --build --preset linux-release` and confirm the Linux `MultidocViewer.wlx64` also reflects the reduced size — **deferred; user will test on a Linux device later**
- [x] 3.3 Record before/after sizes — x64: 57.33→22.26 MB; x86: ~53.7→18.98 MB; documented in design.md and AGENTS.md (`Release sizes` note)
- [x] 3.4 Confirm `BuildMakeSetup.bat` packaging still ships only `MultidocViewer.wlx64`, `MultidocViewer.wlx`, `pluginst.inf` (no ICU DLLs) — regenerated `dist/wlx-multidoc-viewer-Win-20260828.zip` and verified contents