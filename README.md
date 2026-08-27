# WLX Multidoc Viewer

A WLX lister plugin for [Total Commander](https://www.ghisler.com/) and
[Double Commander](https://doublecmd.sourceforge.io/) that displays documents
inside the lister panel: PDF, DjVu, EPUB, FB2, MOBI, XPS, comic archives,
images, and CHM. Heavily inspired by [SumatraPDF reader](https://github.com/sumatrapdfreader/sumatrapdf).

## Features

- **Document formats** — PDF, XPS/OXPS, EPUB, FB2, MOBI, CBZ, DJVU/DJV,
  JPEG, PNG, TIFF, GIF, BMP, WEBP, and CHM (Compiled HTML Help).
  Verified over generated samples ([`examples/`](examples/)) plus
  real-world files.
- **Paged & continuous modes** — single-page view or continuous scrolling with a
  scrollbar that covers the whole document, accurate even for very long or
  high-zoom files. Paged mode pans overflowing pages.
- **Fit & zoom** — fit-to-page / fit-to-width / 100%, zoom in/out, rotation.
- **Toolbar** — navigation, mode and fit toggles, page box, find box, print.
- **Outline sidebar** — table of contents for PDF/EPUB/CHM; the current section
  follows the reading position, click to jump.
- **Text selection & find** — select text with the mouse and copy it, search
  across pages with per-match highlights in documents with a text layer
  (PDF, EPUB, XPS, CHM). Image-only scans and comics stay pan-only; DjVu text
  support is pending a djvulibre build that exports the miniexp API.
- **Printing** — page range and copies through the host print dialog.
- **DPI aware** — renders at the host's effective display scale. No Qt DLLs are
  needed at runtime on Windows.

## Requirements

- **Windows**: Visual Studio 2022 with the C++ workload (or Build Tools),
  CMake 3.20+, and a [vcpkg](https://vcpkg.io/) checkout exposed via the
  `VCPKG_ROOT` environment variable (any location).
- **Linux**: CMake 3.20+, Ninja, Qt 6 (Widgets), and the system development
  packages listed below.

## Dependencies

Everything for Windows comes from the vcpkg manifest (`vcpkg.json`) — no manual
installs. On Linux, install the system packages for your flavor:

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build \
    qt6-base-dev libmupdf-dev libdjvulibre-dev libchm-dev

# Fedora
sudo dnf install gcc-c++ cmake ninja-build \
    qt6-qtbase-devel mupdf-devel djvulibre-devel chmlib-devel

# Arch
sudo pacman -S --needed base-devel cmake ninja qt6-base mupdf djvulibre chmlib
```

(Package names can drift between releases; the CMake script looks for
`Qt6`, `mupdf`, `djvulibre`, and `chm_lib.h`.)

## Building

From a developer shell (on Windows: "Developer PowerShell / Command Prompt for
VS 2022", which provides `cmake` and the MSVC environment):

```bash
# Windows
cmake --preset windows-x64-release
cmake --build --preset windows-release

# Linux
cmake --preset linux-release
cmake --build --preset linux-release
```

### Windows release package

Run `BuildMakeSetup.bat` from a plain console (no developer prompt needed). It
locates MSVC automatically, builds the x64 static release, and packs the plugin
(`wlx-multidoc-viewer.wlx64`) plus `pluginst.inf` into `dist/` with a dated zip
(`dist/wlx-multidoc-viewer-Win-<YYYYMMDD>.zip`).


The plugin binary (`wlx-multidoc-viewer.wlx64`) is written under `build/`
(exact subfolder depends on the preset).

## Installation

Copy `wlx-multidoc-viewer.wlx64` somewhere permanent, then register it as a
lister plugin in your file manager (*Configuration → Options → Plugins → Lister
plugins* in Total Commander; Double Commander has an equivalent lister plugin
page).

On Linux you can instead install directly:

```bash
cmake --install build/linux-release --prefix ~/.local
```

which places the plugin in `~/.local/share/doublecmd/plugins/multidoc/`.

## Usage

Open any supported file in the lister (e.g. `F3` in Total Commander). The
toolbar gives you navigation, display modes, find, and print; the outline
sidebar (toolbar toggle) shows the document's table of contents when one
exists.

### Keys

| Key | Action |
|-----|--------|
| `Right` / `PageDown`, `Left` / `PageUp` | Next / previous page |
| `Up` / `Down` | Scroll line-wise (continuous) |
| `Home` / `End` | First / last page |
| `V` | Toggle paged / continuous mode |
| `Shift+V` | Cycle fit mode (page / width / 100%) |
| `+` / `-` / `0` | Zoom in / out / reset |
| `R` / `Shift+R` | Rotate clockwise / counter-clockwise |
| `G` | Go to page *(Linux viewer)* |
| `Esc` | Clear the text selection; otherwise the host exits the viewer |
| `Ctrl+C` | Copy selected text |
| Mouse wheel | Smooth scroll (continuous) / page turn (paged) |
| Left-drag | Pan, or select text when starting on selectable text |

## Examples

The [`examples/`](examples/) directory contains small generated sample files
(PDF, EPUB, CBZ, images, Markdown) plus a script that regenerates them — handy
for smoke-testing the plugin without hunting for documents. DjVu, XPS, and CHM
samples are best taken from your own files.

## License

See the project repository for license details.
