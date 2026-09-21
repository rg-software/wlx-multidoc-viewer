# Plugin Configuration Specification

## Purpose

Define how the plugin discovers, loads, and exposes a user-editable INI configuration file located in the plugin's own directory. The chrome theme selection and its per-theme palette sections are the primary consumers.

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

### Requirement: Theme selection key

The `[Viewer]` section SHALL support a `Theme` key selecting the chrome palette: `light`, `dark`, or `auto`. The value is case-insensitive. When the key is absent or malformed, the viewer SHALL behave as `auto`.

#### Scenario: Valid Theme in INI
- **WHEN** `multidocviewer.ini` contains `[Viewer] Theme=dark`
- **THEN** the chrome renders with the dark palette on both platforms

#### Scenario: Missing Theme key
- **WHEN** `multidocviewer.ini` is absent or contains no `[Viewer] Theme`
- **THEN** the viewer behaves as `auto`

#### Scenario: Malformed Theme value
- **WHEN** `[Viewer] Theme` contains a non-theme string (e.g. `pink`)
- **THEN** the viewer behaves as `auto`

### Requirement: Theme palette sections

The INI SHALL carry the chrome palette in `[Theme:light]` and `[Theme:dark]` sections, keyed by slot: `PageBackground`, `SidebarBackground`, `ToolbarBackground`, `ToolbarCheckedTint`, `ToolbarCheckedRing`, `Glyph`, `TreeText`, `EditBackground`, `EditText`, `SelectionFill`, `SearchActiveFill`, `SearchActivePen`. Values are hex `#RRGGBB` or `#RRGGBBAA` (alpha); the `#` prefix is optional and hex is case-insensitive. The active theme's section SHALL supply the palette, and any missing or malformed key SHALL fall back to the built-in default for that slot (so an absent or partial section still yields a complete palette).

#### Scenario: Section supplies the active theme
- **WHEN** `Theme=dark` and `[Theme:dark] PageBackground=#102030`
- **THEN** the page area renders `#102030`

#### Scenario: Only the selected theme's section is read
- **WHEN** `Theme=light` and both `[Theme:light]` and `[Theme:dark]` define `PageBackground`
- **THEN** the value from `[Theme:light]` is used

#### Scenario: Missing key falls back to the built-in default
- **WHEN** the active theme's section omits `ToolbarBackground`
- **THEN** the toolbar uses the built-in default for that theme

#### Scenario: Alpha value
- **WHEN** a slot is `#11223344`
- **THEN** it is read as RGB `#112233` with alpha `0x44`