# WLX Multidoc Viewer

A WLX lister plugin for [Total Commander](https://www.ghisler.com/) and
[Double Commander](https://doublecmd.sourceforge.io/) that displays images and numerous electronic document formats. Heavily inspired by [SumatraPDF reader](https://github.com/sumatrapdfreader/sumatrapdf).

## Features

- **Cross-platform**: available on Windows and Linux.

- **Document formats**:

  - fixed-layout (PDF, XPS/OXPS, DJVU/DJV);
  - eBooks (EPUB, FB2, MOBI, CHM);
  - comic books (CBR, CBZ) and multi-page TIFF;
  - images (JPEG, PNG, GIF, BMP).

  Verified over generated `examples` and
  real-world files.

- **Paged & continuous modes**: single-page view or continuous scrolling with a
  scrollbar that covers the whole document, accurate even for very long or
  high-zoom files.

- **Single-page & double-page modes**: show one or two pages side by side.
  Optionally use the double-page mode from the second page of the document.

- **Fit, zoom, & rotation**: fit-to-page / fit-to-width / manual, zoom in/out, rotation.

- **Sidebar**: table of contents for PDF/EPUB/CHM documents and bookmarks.

- **Text selection & find**: select text with the mouse and copy it, search
  across pages with per-match highlights in documents with a text layer
  (PDF, EPUB, XPS, CHM).
  
- **Folder browsing**: navigate between images in the current folder using Next and Previous buttons.

- **Themes**: customizable light and dark modes.

- **Printing**: page range and copies through the host print dialog.

- **DPI aware**: renders at the host's effective display scale.

## Hotkeys

| Key | Action |
| ----- | -------- |
| `Right` / `PgDn`, `Left` / `PgUp` | Next / previous page |
| `Up` / `Down` | Scroll line-wise (continuous) |
| `Home` / `End` | First / last page |
| `V` | Toggle paged / continuous mode |
| `B` | Toggle single / double / skip cover double mode |
| `Shift+B` | Toggle bookmark |
| `Shift+V` | Cycle fit mode (page / width / 100%) |
| `F12` | Show / hide sidebar |
| `+` / `-` / `Numpad 0` or `/` | Zoom in / out / reset |
| `R` / `Shift+R` | Rotate clockwise / counter-clockwise |
| Mouse wheel | Smooth scroll (continuous) / page turn (paged) |
| Left-drag | Pan, or select text when starting on selectable text |

## Requirements

- **Windows**: Visual Studio 2026 with Build Tools,
  CMake 4.2+ (required for the VS 2026 generator), and [vcpkg](https://vcpkg.io/).
- **Linux**: Ninja, Qt 6, and the system packages listed below. CMake 3.25+
  is required for the `--preset` flows; CMake 3.20+ still works for a plain
  `cmake -S . -B build` configure.
- **MuPDF (Linux)**: the MuPDF system package `libmupdf-dev` must be 1.23+
  (meaning Ubuntu 24.04+ or Debian 13+). Note that Ubuntu's MuPDF package lives in the
  `universe` archive, and distributions shipping an older MuPDF can build a
  newer one from the `vcpkg` overlay port.

## Dependencies

Everything for Windows comes from the `vcpkg.json` manifest. On Linux, install the system packages as follows:

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build ca-certificates \
    qt6-base-dev libmupdf-dev libdjvulibre-dev libchm-dev libarchive-dev \
    libfreetype-dev libjpeg-dev zlib1g-dev libopenjp2-7-dev \
    libharfbuzz-dev libpng-dev libjbig2dec0-dev libmujs-dev libgumbo-dev

# Fedora
sudo dnf install gcc-c++ cmake ninja-build ca-certificates \
    qt6-qtbase-devel mupdf-devel djvulibre-devel chmlib-devel libarchive-devel

# Arch
sudo pacman -S --needed base-devel cmake ninja ca-certificates \
    qt6-base mupdf djvulibre chmlib libarchive
```

On Debian/Ubuntu the extra codec packages are required because `libmupdf-dev` is
a static library with no declared dependencies; the build links it via
`pkg-config --static`. The `ca-certificates` package is only needed in minimal build
environments (containers, chroots, sbuild) where a CA bundle may not be
installed. Packages `libarchive-dev`/`libarchive-devel`/`libarchive` provides the
comic-engine archive backend (CBR/CB7).

## Building


```bash
# Windows (MSVS Developer Shell)
BuildMakeSetup.bat

# Linux
./BuildMakeSetup.sh
```
The resulting package will be produced in the `dist` directory.
