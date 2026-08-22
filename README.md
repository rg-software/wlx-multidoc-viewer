# wlx-multidoc-viewer

A WLX lister plugin for [Total Commander](https://www.ghisler.com/) and [Double Commander](https://doublecmd.sourceforge.io/) that displays PDF, DjVu, EPUB, XPS, comic archives, images, and text inside the lister panel.

## Features

- **Formats** (via MuPDF and DjVuLibre): PDF, XPS, OXPS, EPUB, MOBI, FB2, CBZ, CBR, CB7, HTML/HTM, Markdown, TXT, JPEG, PNG, TIFF, GIF, BMP, WEBP, DJVU, DJV.
- **Paged & continuous display modes** — `V` toggles between single-page and continuous scrolling.
- **Smooth scrolling** in continuous mode (mouse wheel, scrollbar, arrow keys, mouse-drag panning) over a virtual canvas whose scrollbar covers the full document — accurate even on very long or high-zoom files. In paged mode, an overflowing page (wider/taller than the window) gains a scrollbar and drag-panning to move the visible part.
- **Fit modes** — `Shift+V` cycles fit-to-page → fit-to-width → 100%. `+`/`-`/`0` adjust zoom.
- **Rotation** — `R` rotates 90° clockwise, `Shift+R` counter-clockwise; persisted per document.
- **Navigation** — `Right`/`PageDown` next, `Left`/`PageUp` prev, `Home` first, `End` last; `G` go-to-page (Qt viewer).
- **Info panel** at the top showing `current / total`, continuous status, and fit/zoom state.
- **DPI aware** — rendered at the host's effective display scale; no Qt DLLs needed at runtime on Windows.
- **Per-page render cache** — continuous mode paints only visible pages from a bounded LRU cache, keeping memory flat and scroll cost proportional to the viewport.

## Requirements

### Windows

- [Visual Studio 2022](https://visualstudio.microsoft.com/) (C++ workload)
- [vcpkg](https://vcpkg.io/) at `C:\vcpkg` (uses the `x64-windows-static-md` triplet)
- CMake 3.20+

### Linux

- CMake 3.20+ and Ninja
- Qt 6 (Widgets)
- System `libmupdf` and `libdjvulibre` development packages
- A C++17 compiler (GCC or Clang)

## Building

### Windows

Run from a VS 2022 developer shell (vcvarsall):

```bash
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 && cmake --preset windows-x64-release && cmake --build --preset windows-release'
```

Output: `build/release/Release/wlx-multidoc-viewer.wlx64`

### Linux

Install system dependencies first. On Debian/Ubuntu:

```bash
sudo apt install cmake ninja-build qt6-base-dev libmupdf-dev libdjvulibre-dev
```

On Arch/CachyOS:

```bash
sudo pacman -S --needed cmake ninja qt6-base mupdf djvulibre
```

Build:

```bash
cmake --preset linux-release
cmake --build --preset linux-release
```

Output: `build/linux-release/wlx-multidoc-viewer.wlx64`

Install (user-local, no root):

```bash
cmake --install build/linux-release --prefix ~/.local
```

This places the plugin at `~/.local/share/doublecmd/plugins/multidoc/wlx-multidoc-viewer.wlx64`. For a system-wide install, use `--prefix /usr` instead (lands in `/usr/share/doublecmd/plugins/multidoc/`).

### Test harnesses (optional, Windows)

The project builds two real-input harnesses with `-DWLX_BUILD_HARNESS=ON`:

- `harness-scroll` — validates the continuous-mode virtual canvas (scroll range, mid-document reach, mixed page sizes, viewport anchoring).
- `harness-win` — drives real mouse/keyboard input to test drag scrolling, cursor state, and scroll clamping.

## Installation

On Linux, prefer `cmake --install build/linux-release --prefix ~/.local` (see [Building](#building)); it installs to `<prefix>/share/doublecmd/plugins/multidoc/`. Otherwise copy `wlx-multidoc-viewer.wlx64` manually. Then register the plugin in the file manager: add it as a lister plugin via *Configuration → Options → Plugins → Lister plugins* (Total Commander); Double Commander similarly registers `.wlx64` plugins in its plugin settings.

## Usage

Open any supported file in the lister (e.g. press `F3` in Total Commander). Use the info panel at the top and the keys below:

| Key | Action |
|-----|--------|
| `Right` / `PageDown` | Next page |
| `Left` / `PageUp` | Previous page |
| `Home` / `End` | First / last page |
| `V` | Toggle paged / continuous |
| `Shift+V` | Cycle fit mode |
| `+` / `-` / `0` | Zoom in / out / 100% |
| `R` / `Shift+R` | Rotate CW / CCW |
| `G` | Go to page (Qt viewer) |
| `Esc` | Exit the viewer (Qt viewer forwards a `Q` keypress to the host) |
| Mouse wheel | Smooth scroll (continuous) / page turn (paged) |
| Left-drag | Pan (continuous; paged when page overflows) |

## Keyboard shortcuts map

Single source in `ViewerController`; identical across platforms.

## Architecture

```
src/
  plugin.cpp            WLX entry points (ListLoad, ListCloseWindow, etc.)
  wlxplugin.h           WLX API types and DCPCALL macro
  document.h            DocumentEngine interface (open/render/text/outline)
  formatdispatcher.cpp  Routes file extensions to the right engine
  mupdfengine.*         MuPDF backend (PDF, XPS, EPUB, images, HTML)
  djvuengine.*          DjVuLibre backend (DJVU, DJV)
  viewercontroller.*    Shared state + commands + virtual-canvas layout + render cache
  viewer_win32.*        Win32 viewer (Windows) — pure HWND, per-page BitBlt paint
  viewer.*              Qt viewer (Linux) — QFrame, QScrollArea, ViewerCanvas widget
```

## License

See the project repository for license details.