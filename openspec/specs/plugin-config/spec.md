# Plugin Configuration Specification

## Purpose

Define how the plugin discovers, loads, and exposes a user-editable INI configuration file located in the plugin's own directory. The first consumer is the page-area background color.

## Requirements

### Requirement: INI file location and format

The plugin SHALL read a file named `multidocviewer.ini` from the same directory as the loaded plugin binary (DLL on Windows, `.so` on Linux). The file uses standard INI syntax: `[section]` headers, `key=value` pairs, `;`-prefixed comments. The plugin MUST NOT write to this file at runtime.

#### Scenario: INI file present
- **WHEN** `multidocviewer.ini` exists in the plugin directory
- **THEN** the plugin parses it on first access and exposes the values through its config singleton

#### Scenario: INI file absent
- **WHEN** `multidocviewer.ini` does not exist in the plugin directory
- **THEN** the parse produces an empty structure and all settings fall back to their compile-time defaults

### Requirement: Lazy load and cache

The INI file SHALL be parsed exactly once, on first access to the config singleton. The parsed result is cached for the lifetime of the process. Edits to the file on disk after the initial parse are not observed.

#### Scenario: First access triggers parse
- **WHEN** any code first calls the config accessor
- **THEN** the INI file is read and parsed (or an empty structure is used if the file is absent), and subsequent calls return the cached result without re-reading

### Requirement: Plugin directory resolution

The plugin SHALL resolve its own directory from the loaded module path, not from the host-supplied `DefaultIniName`. On Windows this uses `GetModuleHandle`/`GetModuleFileName`; on Linux it uses `dladdr`. `ListSetDefaultParams` remains a no-op.

#### Scenario: Windows module path
- **WHEN** the plugin loads on Windows
- **THEN** the directory of the `.wlx` / `.wlx64` DLL is used as the INI base path

#### Scenario: Linux module path
- **WHEN** the plugin loads on Linux
- **THEN** the directory of the `.wlx64` shared object (resolved via `dladdr`) is used as the INI base path

### Requirement: Background color configuration

The `[Viewer]` section SHALL support a `BackgroundColor` key specifying the page-area background as a hex RGB value (e.g. `#E8E8E8` or `E8E8E8`). The value is case-insensitive and the `#` prefix is optional. When the key is absent or malformed, the default `0xE8E8E8` (light gray) is used.

#### Scenario: Valid background color in INI
- **WHEN** `multidocviewer.ini` contains `[Viewer] BackgroundColor=#FFFFFF`
- **THEN** the page area renders with a white background on both platforms

#### Scenario: Missing BackgroundColor key
- **WHEN** `multidocviewer.ini` is absent or contains no `[Viewer] BackgroundColor`
- **THEN** the page area renders with the default light-gray background (`0xE8E8E8`)

#### Scenario: Malformed BackgroundColor value
- **WHEN** `[Viewer] BackgroundColor` contains a non-hex string (e.g. `red`)
- **THEN** the page area falls back to the default `0xE8E8E8`

### Requirement: Sidebar background color configuration

The `[Viewer]` section SHALL support a `SidebarBackground` key specifying the outline sidebar's background as a hex RGB value (e.g. `#F0F0F0` or `F0F0F0`). The value is case-insensitive and the `#` prefix is optional. When the key is absent or malformed, the sidebar SHALL use the default `0xE8E8E8` (light gray). The sidebar setting SHALL NOT affect the page-area background, and the page-area `BackgroundColor` setting SHALL NOT affect the sidebar.

#### Scenario: Valid sidebar background color in INI
- **WHEN** `multidocviewer.ini` contains `[Viewer] SidebarBackground=#F0F0F0`
- **THEN** the outline sidebar renders with the `#F0F0F0` background on both platforms, and the page area keeps its own configured background

#### Scenario: Missing SidebarBackground key
- **WHEN** `multidocviewer.ini` is absent or contains no `[Viewer] SidebarBackground`
- **THEN** the sidebar renders with the default light-gray background (`0xE8E8E8`)

#### Scenario: Malformed SidebarBackground value
- **WHEN** `[Viewer] SidebarBackground` contains a non-hex string (e.g. `sidebar`)
- **THEN** the sidebar falls back to the default `0xE8E8E8`

#### Scenario: Sidebar setting leaves page background untouched
- **WHEN** `multidocviewer.ini` sets `[Viewer] SidebarBackground` to a color different from `BackgroundColor`
- **THEN** the page area and the sidebar each render with their own configured colors