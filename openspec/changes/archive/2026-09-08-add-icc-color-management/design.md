## Context

See `proposal.md` — Why/What. Implementation facts that shape the approach:

- The viewer renders MuPDF pages through `MuPdfEngine::renderPage`, which calls `fz_new_pixmap_from_page(m_ctx, fzpage, ctm, fz_device_rgb(m_ctx), 0)` (src/mupdfengine.cpp:332) and copies `pixmap->samples` into a `QImage::Format_RGB888`. The conversion to RGB happens inside MuPDF with the current `fz_colorspace`; whether CMYK/ICC content is color-managed depends entirely on whether the MuPDF build has ICC (lcms2) enabled.
- MuPDF gates ICC at compile time via `FZ_ENABLE_ICC` (default 1 in upstream `include/mupdf/fitz/config.h`). `source/fitz/color-lcms.c` compiles `#if FZ_ENABLE_ICC`, `#include "lcms2.h"`, and uses it for profiles and CMYK/RGB conversion. When `FZ_ENABLE_ICC=0`, MuPDF's `fz_css`/CMYK→RGB path falls back to a naive component-wise `1-min(1, c+k)` formula.
- The overlay port (`overlay-ports/libmupdf/CMakeLists.txt`) currently defines `FZ_ENABLE_ICC=0` with comment "needs lcsm2", and Thrift spins from `trim-binary-size` which deliberately dropped it. MuPDF 1.28.3's `source/fitz/load-jpeg.c` reads JPEG EXIF/ICC markers and attaches an `fz_colorspace` (ICCBased) when ICC is compiled in — that is exactly what `AC3_GW_Notebook_GER.pdf`'s title image carries.
- Windows builds MuPDF from the overlay port via vcpkg (static, `x64-windows-static-md`). Linux uses the distro's shared `libmupdf` (CMakeLists.txt:35 `find_library(MUPDF_LIBRARY NAMES mupdf)`) which upstream ships with ICC enabled — so this design changes Windows only.
- The vcpkg `lcms` port (Little CMS 2.19.x) installs both `lcms2-config.cmake` (defines imported target `lcms2::lcms2`) and `lcms-config.cmake`. No in-tree code references lcms2 today. The overlay port's `unofficial-libmupdf-config.cmake.in` lists the static-branch `find_dependency` calls; adding `lcms2::lcms2` to the link interface without a matching `find_dependency(lcms2 CONFIG)` makes the consuming project's `find_package(unofficial-libmupdf)` fail on the unresolved transitive target (observed during apply; fixed in task 1.5).

## Goals / Non-Goals

**Goals:**
- Make CMYK/ICC-tagged content render with its source colors on Windows via MuPDF's normal ICC pipeline, without touching engine code.
- Keep the change confined to the libmupdf overlay port (+ the lcms2 dep), so `renderPage` logic stays identical.
- Keep the size cost as low as the fix allows (lcms2 static is ~0.4–1 MB; no extra feature flags).

**Non-Goals:**
- No custom ICC/color-profile handling in the viewer: no banner "proofing" UI, no monitor profile input, no per-document ICC injection. MuPDF already ships the lcms2-based machinery; we only re-enable it.
- No change to DjVu/CDV/CHM/comic/image engines (the bug is MuPDF-specific; CHM's HTML pipeline also goes through MuPDF's colorspaces, so it inherits the fix).
- No Linux changes: distro MuPDF already has ICC.

## Decisions

**D1 — Enable ICC at the MuPDF build via `FZ_ENABLE_ICC=1` in the overlay port.**
Flip the definition in `overlay-ports/libmupdf/CMakeLists.txt` from `FZ_ENABLE_ICC=0` to `=1` (or drop it, since 1 is the upstream default — but keep it explicit with an updated comment so the trim-binary-size intent is documented as deliberately reversed). Add `find_package(lcms2 CONFIG REQUIRED)` beside the other `find_package` calls and `lcms2::lcms2` to `target_link_libraries(libmupdf PRIVATE ...)`.
*Alternatives rejected:* (a) post-processing CMYK QImages in `mupdfengine.cpp` with a hand-rolled matrix — duplicates what MuPDF+lcms2 does, is color-wrong for arbitrary profiles, and leaks engine knowledge into the viewer; (b) waiting for MuPDF to gain a dynamically-loaded lcms2 — no such hook exists in 1.28.x.

**D2 — Add `lcms` to `overlay-ports/libmupdf/vcpkg.json` dependencies.**
The overlay port's manifest already drives the transitive build deps (freetype, harfbuzz, libjpeg-turbo, openjpeg, gumbo, jbig2dec, zlib). `lcms` slots straight in; vcpkg resolves the static `x64-windows-static-md` build from its own instance at `C:\vcpkg`. No host-tool dep needed.
*Alternatives rejected:* referencing a system `find_library(lcms2)` — breaks the vcpkg-managed Windows build hermeticity and would drag dl/threading variants.

**D3 — Keep the viewer's render path unchanged.**
`renderPage` still calls `fz_new_pixmap_from_page(..., fz_device_rgb, ...)`. With ICC compiled in, MuPDF uses the embedded profile (`fz_cal_color`/lcms2) during that conversion; RGB/grayscale documents without profiles produce identical output as before (the naive fallback only ever applied to CMYK/ICC content, and the engine default for profile-less content is unchanged).
*Alternatives rejected:* asking for an explicit `fz_new_icc()` colorspace in the engine — changes the public `DocumentEngine` behavior contract for no benefit.

**D4 — Verify with the exact failing artifact, not a synthetic one.**
The acceptance check uses `examples/AC3_GW_Notebook_GER.pdf` itself: page 1 must render as dark brown with a visible beige pattern, not near-black red. Verification is automated headlessly: a `tests/harness_icc.cpp` smoke harness (built like `harness-mobi`) opens the real PDF through `MuPdfEngine`, renders page 1 at 0.25 zoom, downsamples every 16th pixel, and asserts brown pixels appear and the red-collapse signature (`G==0 && B==0 && R>8`, characteristic of the no-ICC fallback) is absent. It skips (exit 0) when the large sample PDF is not present so the target stays CI-friendly. Keep the existing sample-image generators unchanged; no new `examples/` file.

## Risks / Trade-offs

- [Binary-size growth on Windows (~1.8 MB; lcms2 static is 3.34 MB)] → Accepted trade: the fix restores color correctness; if a future size pass needs to reclaim it, the knob is still the same single `FZ_ENABLE_ICC` line, now documented as deliberately on.
- [lcms2 thread-safety / static-init cost] → lcms2 is thread-safe after `cmsSetErrorHandler`/first use; MuPDF initializes lcms once per context (`fz_new_context` already statically initializes lcms globals when ICC is enabled). Single-context use in `MuPdfEngine` is unchanged (mutex already guards `m_ctx`).
- [Behavioral change for documents that *want* the naive conversion] → No such intent exists; the fallback was a size-driven artifact. Profile-less content keeps old output (spec scenario "JPEG without embedded profile"). Additive risk limited to CMYK documents that were previously unreadable (black) now showing something.
- [Linux distro MuPDF could theoretically disable ICC] → Not in practice (Debian/Ubuntu/Fedora build with lcms2); out of scope regardless — the Linux path is unchanged and the spec is satisfied by the default distro build.

## Migration Plan

- No persisted data, config, or API to migrate. A rollback is a straight revert of the two overlay-port files, followed by a libmupdf rebuild.
- Ship order: (1) `vcpkg.json` add `lcms`; (2) `CMakeLists.txt` `FZ_ENABLE_ICC=1` + `find_package` + link; (3) rebuild the libmupdf port via vcpkg (VS dev shell + `VCPKG_ROOT`); (4) rebuild the plugin preset and verify with the failing PDF.

## Platform-Specific Code

- **Windows (`overlay-ports/libmupdf/CMakeLists.txt`, `vcpkg.json`, `unofficial-libmupdf-config.cmake.in`):** `FZ_ENABLE_ICC=1`, `find_package(lcms2 CONFIG REQUIRED)`, link `lcms2::lcms2` under `libmupdf`'s `target_link_libraries`, add `"lcms"` under the port's `dependencies` in `vcpkg.json`, and add `find_dependency(lcms2 CONFIG)` to the static branch of `unofficial-libmupdf-config.cmake.in` (without it the consuming `find_package(unofficial-libmupdf)` fails to resolve the transitive static target). Both debug and release variants are resolved by vcpkg's per-config install trees (the port does `vcpkg_cmake_install` per config) — no manual release/debug splitting needed, unlike CHMLib/LibArchive. Verification harness: `tests/harness_icc.cpp` wired as `harness-icc` under `WLX_BUILD_HARNESS` in the root `CMakeLists.txt`.
- **Linux (none):** no file changes; the distro `libmupdf` already links lcms2 and satisfies the spec delta without code involvement.