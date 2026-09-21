## ADDED Requirements

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

## REMOVED Requirements

### Requirement: Background color configuration

**Reason**: Superseded by the theme palette's `PageBackground` slot; a flat page color with no theme notion cannot express a light/dark pair.

**Migration**: Set `[Theme:light] PageBackground` and/or `[Theme:dark] PageBackground` instead. The old `[Viewer] BackgroundColor` key is ignored.

### Requirement: Sidebar background color configuration

**Reason**: Superseded by the theme palette's `SidebarBackground` slot.

**Migration**: Set `[Theme:light] SidebarBackground` and/or `[Theme:dark] SidebarBackground` instead. The old `[Viewer] SidebarBackground` key is ignored.
