# WLX Multidoc Viewer

A WLX lister plugin for [Total Commander](https://www.ghisler.com/) and
[Double Commander](https://doublecmd.sourceforge.io/) that displays images and numerous electronic document formats. Heavily inspired by [SumatraPDF reader](https://github.com/sumatrapdfreader/sumatrapdf).

## Features

- **Cross-platform**: available on Windows and Linux.

- **Document formats**:

  - fixed-layout (PDF, XPS/OXPS, DJVU/DJV);
  - eBooks (EPUB, FB2, MOBI, CHM);
  - comic books (CBR, CBZ);
  - images (JPEG, PNG, TIFF, GIF, BMP, WEBP).

  Verified over generated `examples` and
  real-world files.

- **Paged & continuous modes**: single-page view or continuous scrolling with a
  scrollbar that covers the whole document, accurate even for very long or
  high-zoom files.

- **Fit & zoom**: fit-to-page / fit-to-width / 100%, zoom in/out, rotation.

- **Outline sidebar**: table of contents for PDF/EPUB/CHM documents.

- **Text selection & find**: select text with the mouse and copy it, search
  across pages with per-match highlights in documents with a text layer
  (PDF, EPUB, XPS, CHM).

- **Printing**: page range and copies through the host print dialog.

- **DPI aware**: renders at the host's effective display scale.

## Hotkeys

| Key | Action |
| ----- | -------- |
| `Right` / `PgDn`, `Left` / `PgUp` | Next / previous page |
| `Up` / `Down` | Scroll line-wise (continuous) |
| `Home` / `End` | First / last page |
| `V` | Toggle paged / continuous mode |
| `Shift+V` | Cycle fit mode (page / width / 100%) |
| `+` / `-` / `0` | Zoom in / out / reset |
| `R` / `Shift+R` | Rotate clockwise / counter-clockwise |
| Mouse wheel | Smooth scroll (continuous) / page turn (paged) |
| Left-drag | Pan, or select text when starting on selectable text |

## Requirements

- **Windows**: Visual Studio 2022 with Build Tools,
  CMake 3.20+, and [vcpkg](https://vcpkg.io/).
- **Linux**: CMake 3.20+, Ninja, Qt 6, and the system packages listed below.

## Dependencies

Everything for Windows comes from the `vcpkg.json` manifest. On Linux, install the system packages as follows:

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

## Building


```bash
# Windows (MSVS Developer Shell)
BuildMakeSetup.bat

# Linux
./BuildMakeSetup.sh
```
The resulting package will be produced in the `dist` directory.
