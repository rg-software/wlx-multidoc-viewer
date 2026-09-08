## Why

The plugin's page area already has a configurable background color (`[Viewer] BackgroundColor`), but the outline sidebar still uses the platform default (system window color on Win32, default Qt palette on Linux). On themed setups — especially dark page backgrounds — the sidebar clashes with the reading area. A `SidebarBackground` INI setting lets users match the sidebar to the page background.

## What Changes

- New `[Viewer] SidebarBackground` INI key specifying the sidebar background as a hex RGB value (`#RRGGBB` or `RRGGBB`, case-insensitive, `#` prefix optional) — the same format `BackgroundColor` already uses.
- When the key is absent or malformed, the sidebar falls back to the default `0xE8E8E8` (light gray, matching the page-area default).
- The Win32 sidebar (panel window class brush, grip area) paints with the configured color.
- The Qt sidebar (`QTreeWidget` + grip widget palette) renders with the configured color.
- The template `assets/multidocviewer.ini` documents the new key with a commented example.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `plugin-config`: Add a sidebar background color configuration requirement — a new `[Viewer] SidebarBackground` key whose value is consumed by both platforms' sidebar rendering. This extends the existing INI-fed `[Viewer]` settings (`BackgroundColor`, `SidebarWidth`, `SidebarVisible`).

## Impact

- `src/viewer_settings.h` — new `kSidebarBackground` constant initialized from the config singleton.
- `src/sidebar_win32.cpp` — replace the `COLOR_WINDOW`/`COLOR_BTNFACE` class brushes with a brush built from the configured color.
- `src/sidebar_qt.cpp` — set a `QPalette` on the tree and grip from the configured color.
- `assets/multidocviewer.ini` — document the new key in the shipped template.
- No WLX API, engine, or dependency changes.