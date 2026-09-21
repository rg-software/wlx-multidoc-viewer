## MODIFIED Requirements

### Requirement: Theme palette sections

The INI SHALL carry the chrome palette in `[Theme:light]` and `[Theme:dark]` sections, keyed by slot: `PageBackground`, `SidebarBackground`, `ToolbarBackground`, `ToolbarCheckedTint`, `ToolbarCheckedRing`, `Glyph`, `TreeText`, `EditBackground`, `EditText`, `SelectionFill`, `SearchActiveFill`, `SearchActivePen`, `DocumentBackground`, `DocumentText`. Values are hex `#RRGGBB` or `#RRGGBBAA` (alpha); the `#` prefix is optional and hex is case-insensitive. The active theme's section SHALL supply the palette, and any missing or malformed key SHALL fall back to the built-in default for that slot (so an absent or partial section still yields a complete palette).

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
