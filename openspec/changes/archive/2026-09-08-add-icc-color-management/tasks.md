## 1. Enable ICC in the libmupdf overlay port

- [x] 1.1 In `overlay-ports/libmupdf/vcpkg.json`, add `"lcms"` to the `dependencies` array (alongside freetype, harfbuzz, libjpeg-turbo, etc.).
- [x] 1.2 In `overlay-ports/libmupdf/CMakeLists.txt`, change the `FZ_ENABLE_ICC=0` compile definition to `FZ_ENABLE_ICC=1` and update its inline comment to note the ICC dependency is deliberate (color correctness for CMYK/ICC content; the size trade-off of lcms2 is accepted).
- [x] 1.3 In `overlay-ports/libmupdf/CMakeLists.txt`, add `find_package(lcms2 CONFIG REQUIRED)` to the dependency block (near the other `find_package` calls).
- [x] 1.4 In `overlay-ports/libmupdf/CMakeLists.txt`, add `lcms2::lcms2` to `target_link_libraries(libmupdf PRIVATE ...)`.
- [x] 1.5 In `overlay-ports/libmupdf/unofficial-libmupdf-config.cmake.in`, add `find_dependency(lcms2 CONFIG)` to the static-branch dependency list so the consuming plugin resolves `lcms2::lcms2` transitively.

## 2. Rebuild and verify on Windows

- [x] 2.1 Rebuild the `libmupdf` vcpkg port from a VS developer shell with `VCPKG_ROOT` set (vcpkg manifest/overlay mode picks up the changed `vcpkg.json` + `CMakeLists.txt`).
- [x] 2.2 Rebuild the plugin preset (`cmake --preset windows-x64-release && cmake --build --preset windows-release`); confirmed the binary grew only by the lcms2 static footprint (22 MB → 23.78 MB).
- [x] 2.3 **Headless** verification (done): add `tests/harness_icc.cpp` + `harness-icc` CMake target (WLX_BUILD_HARNESS, like `harness-mobi`); renders page 1 of `AC3_GW_Notebook_GER.pdf` through the real engine and asserts brown pixels (not the near-black red-collapse fallback). Run result: all sampled pixels brown, zero red-collapse → **ALL PASS**.
- [x] 2.4 **Interactive** smoke check: opened `examples/AC3_GW_Notebook_GER.pdf` in Total Commander; page 1 shows the dark-brown title image with the beige pattern visible (not near-black red). `examples/sample.pdf` and grayscale pages render with no color shift.
- [x] 2.5 Record the resulting binary size (23.78 MB) and the delta vs. the pre-change size; noted in proposal/design Impact and Risks sections.

## 3. Linux regression

- [x] 3.1 No-op: the change is confined to the libmupdf vcpkg overlay port (Windows-only; Linux links the distro MuPDF which already ships ICC/lcms2). No Linux-relevant files were touched (`CMakeLists.txt` harness target and overlay-port edits are Windows-guarded), so the Linux preset is unaffected. A `cmake --preset linux-release` re-run is only needed if a full Linux smoke is ever desired.