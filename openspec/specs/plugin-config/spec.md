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

The INI SHALL carry the chrome palette in `[Theme:light]` and `[Theme:dark]` sections, keyed by slot: `PageBackground`, `SidebarBackground`, `ToolbarBackground`, `ToolbarCheckedTint`, `ToolbarCheckedRing`, `Glyph`, `TreeText`, `EditBackground`, `EditText`, `SelectionFill`, `SearchActiveFill`, `DocumentBackground`, `DocumentText`. Values are hex `#RRGGBB` or `#RRGGBBAA` (alpha); the `#` prefix is optional and hex is case-insensitive. The active theme's section SHALL supply the palette, and any missing or malformed key SHALL fall back to the built-in default for that slot (so an absent or partial section still yields a complete palette).

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

#### Scenario: Document slots are read
- **WHEN** `[Theme:dark]` sets `DocumentBackground=#202020` and `DocumentText=#DDDDDD`
- **THEN** themed document bodies use those colors

### Requirement: Autosave view-mode flag key

The `[Viewer]` section SHALL support a boolean `AutosaveViewMode` key that turns on saving and restoring of the last-used view mode (see `view-mode-persistence`). Accepted values are case-insensitive: `1`, `true`, `yes`, `on` enable it; `0`, `false`, `no`, `off` disable it. When the key is absent or malformed, autosave SHALL be disabled. The key is read-only like every other INI key: the plugin MUST NOT write it back.

#### Scenario: Flag enabled
- **WHEN** `multidocviewer.ini` contains `[Viewer] AutosaveViewMode=true`
- **THEN** view-mode autosave is active for the process

#### Scenario: Flag absent
- **WHEN** `multidocviewer.ini` contains no `[Viewer] AutosaveViewMode` key
- **THEN** autosave is disabled and behavior matches a build without the feature

#### Scenario: Malformed flag value
- **WHEN** `[Viewer] AutosaveViewMode` contains an unparseable string (e.g. `maybe`)
- **THEN** autosave is disabled

### Requirement: Ship configuration as template
Release and install packaging SHALL ship the default configuration as `multidocviewer.ini.sample`, never as a live `multidocviewer.ini`.

#### Scenario: Package contents
- **WHEN** a release archive or install image is produced (Windows or Linux)
- **THEN** it contains `multidocviewer.ini.sample` with the documented defaults and does not contain `multidocviewer.ini`

### Requirement: Preserve existing configuration on update
The system SHALL NOT overwrite an existing `multidocviewer.ini` when a newer version of the plugin is installed.

#### Scenario: Update with existing INI
- **WHEN** the plugin is installed or updated into a directory that already contains `multidocviewer.ini`
- **THEN** the existing `multidocviewer.ini` remains unchanged (the package carries only the `.sample`)

### Requirement: Absent INI keeps working
A missing `multidocviewer.ini` SHALL continue to mean compile-time defaults; the viewer SHALL NOT require the user to create the file.

#### Scenario: Fresh install without customization
- **WHEN** only `multidocviewer.ini.sample` is present and no `multidocviewer.ini` exists
- **THEN** the viewer behaves exactly as with an absent INI (all defaults)

### Requirement: Sample file as fallback layer
The plugin SHALL read `multidocviewer.ini.sample` from the plugin directory as the base configuration layer when present, and overlay `multidocviewer.ini` on top of it key by key. Read sites keep their hardcoded fallbacks for keys absent from both files.

#### Scenario: Sample supplies a missing user key
- **WHEN** the user INI sets `[Viewer] SidebarWidth` but no `Theme`
- **THEN** `SidebarWidth` resolves from the user INI and `Theme` resolves from the sample

#### Scenario: Sample absent
- **WHEN** neither `multidocviewer.ini` nor `multidocviewer.ini.sample` exists
- **THEN** every key resolves to its hardcoded read-site default, as before