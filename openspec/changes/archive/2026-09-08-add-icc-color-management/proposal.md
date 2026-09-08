## Why

Embedded **CMYK JPEGs render near-black** in the Windows reader. `examples/AC3_GW_Notebook_GER.pdf` opens with a dark-brown title-page image (beige geometric pattern); the viewer shows it as very dark red / almost black with the pattern invisible. Root cause: the libmupdf vcpkg overlay port builds with `FZ_ENABLE_ICC=0` (set during `trim-binary-size` to skip the lcms2 dep), so MuPDF converts CMYK→RGB with a naive `1-min(1,c+k)` fallback instead of honoring the image's embedded ICC color profile. Sample conversion: proper ICC yields brown `(34,24,19)`, the fallback collapses ~97% of pixels to `(0,0,0)`. Linux is unaffected — it links the distro MuPDF, which ships ICC support.

## What Changes

- **Enable ICC in the libmupdf overlay port** (`overlay-ports/libmupdf/`): set `FZ_ENABLE_ICC=1` in `CMakeLists.txt`, add `find_package(lcms2 CONFIG REQUIRED)`, link `lcms2::lcms2`, and add the `lcms` port to `vcpkg.json` (`overlay-ports/libmupdf/vcpkg.json`).
- Requires a libmupdf vcpkg rebuild (VS dev shell + `VCPKG_ROOT`), then the plugin rebuild.
- Windows-only: no `CMakeLists.txt` change outside the overlay port, no engine/plugin source changes, no spec behavior beyond rendering correct colors.
- **Trade-off:** adds `lcms` (Little CMS) to the static link. Measured on Windows: `MultidocViewer.wlx64` grew from ~22 MB to **23.78 MB** (~1.8 MB; lcms2 static is 3.34 MB but links in ~1.8 MB). Cost is deliberate: rendering CMYK/ICC images with wrong color is a correctness bug, not a size trade subjects accept. Also requires a new `find_dependency(lcms2 CONFIG)` in the overlay port's config template so the consuming build resolves the transitive target.

## Capabilities

### New Capabilities
- none

### Modified Capabilities
- `viewer-rendering`: MuPDF-backed pages SHALL honor embedded ICC color profiles when converting to RGB, instead of the naive component-wise fallback.

## Impact

- Build: `overlay-ports/libmupdf/CMakeLists.txt`, `overlay-ports/libmupdf/vcpkg.json`; libmupdf port + plugin rebuild on Windows.
- Dependency added: `lcms` (Little CMS 2.19.x, static, x64-windows-static-md).
- Artifact size: Windows x64 increases (~0.4–1 MB, lcms2 static); Linux unchanged (system MuPDF already has ICC).
- No change to the WLX API, `DocumentEngine` interface, or any runtime contract; `renderPage` output for CMYK/ICC content on Windows becomes color-correct.