# Viewer Theme Specification

## Purpose

Defines how the plugin derives and applies its chrome color palette — page background, sidebar, toolbar, and text fields — so the lister matches the ambient light or dark mode of its host on both platforms, with the palette values themselves supplied by the INI theme sections.

## Requirements

### Requirement: Theme source and selection

The viewer SHALL render its chrome from a palette selected by the `[Viewer] Theme` key in the plugin's INI, accepting `light`, `dark`, or `auto`, and SHALL behave as `auto` when the key is absent or malformed. In `auto` mode the viewer SHALL follow the host's ambient mode: dark when the host signals dark mode (Windows: the dark-mode flag passed to a load entry point; Linux: the Qt color-scheme hint), light otherwise. The palette's slot values SHALL come from the selected theme's INI section (`[Theme:light]` or `[Theme:dark]`, per the `plugin-config` capability), falling back to built-in defaults for missing keys. The active palette SHALL be resolved once per process at first viewer construction and remain frozen for the process lifetime.

#### Scenario: INI selects dark
- **WHEN** `[Viewer] Theme=dark`
- **THEN** the chrome renders with the dark palette regardless of the host's mode

#### Scenario: INI selects light
- **WHEN** `[Viewer] Theme=light`
- **THEN** the chrome renders with the light palette regardless of the host's mode

#### Scenario: auto follows a dark host on Windows
- **WHEN** the plugin loads on Windows with `[Viewer] Theme` absent and the host passes the dark-mode flag to a load entry point
- **THEN** the chrome renders with the dark palette

#### Scenario: auto follows a light host on Linux
- **WHEN** the plugin loads on Linux with `[Viewer] Theme` absent and the Qt color-scheme hint is light
- **THEN** the chrome renders with the light palette

#### Scenario: malformed Theme value
- **WHEN** `[Viewer] Theme` contains an unrecognized value
- **THEN** the viewer behaves as `auto`

#### Scenario: palette values come from the theme section
- **WHEN** the active theme is dark and `[Theme:dark]` sets `PageBackground=#102030`
- **THEN** the page area renders `#102030`

#### Scenario: theme frozen per process
- **WHEN** multiple viewer windows open in one process
- **THEN** all of them render with the same, first-resolved palette

### Requirement: Palette slots

The palette SHALL define values for the page-area background, sidebar background, toolbar background, toolbar checked-state tint, toolbar checked-state ring, toolbar icon glyph, sidebar tree text, toolbar edit-field background, and toolbar edit-field text. The `PageBackground` and `SidebarBackground` slots SHALL replace the retired flat `[Viewer] BackgroundColor`/`SidebarBackground` keys as the single source for those surfaces. The light palette SHALL match the plugin's pre-theme default appearance on both platforms.

#### Scenario: all slots present
- **WHEN** a theme is active
- **THEN** every chrome surface listed above renders from a value in that theme's palette

#### Scenario: light palette parity
- **WHEN** the light palette is active
- **THEN** the page-area and sidebar backgrounds match the pre-theme defaults on both platforms

#### Scenario: retired keys are ignored
- **WHEN** the INI still contains `[Viewer] BackgroundColor` or `[Viewer] SidebarBackground`
- **THEN** those keys have no effect; the theme section's `PageBackground`/`SidebarBackground` are used

### Requirement: Document body theming

The viewer SHALL theme document bodies from the palette's `DocumentBackground` and `DocumentText` slots, following the active theme with no separate switch. Reflowable documents (EPUB, MOBI, FB2, HTML, CHM) SHALL be styled with an internal stylesheet built from those slots so the body renders against the theme background with theme text. Fixed-layout documents (PDF, XPS, TIFF, DjVu) SHALL be recolored per rendered page: a page that is effectively monochrome SHALL be remapped by luminance between `DocumentText` and `DocumentBackground`; a page carrying real color SHALL be left unchanged. Raster images and comic pages SHALL never be recolored.

#### Scenario: reflowable body follows the dark theme
- **WHEN** the dark palette is active and an EPUB is open
- **THEN** its body renders on the dark document background with light text

#### Scenario: monochrome fixed-layout page is recolored
- **WHEN** the dark palette is active and a PDF page is effectively black-on-white
- **THEN** the page is remapped to the dark background and light text

#### Scenario: colored fixed-layout page is preserved
- **WHEN** the dark palette is active and a PDF page contains significant color
- **THEN** that page renders in its own colors

#### Scenario: images and comics are never recolored
- **WHEN** the dark palette is active and an image or comic page is shown
- **THEN** it renders in its own colors

#### Scenario: light theme is a no-op
- **WHEN** the light palette is active
- **THEN** document bodies render as they would without theming

### Requirement: Document and overlay rendering unaffected by theme

The rendered document bitmaps SHALL be themed per the `Document body theming` requirement. The print output SHALL NOT change with the theme. The text-selection and search-match overlay colors SHALL be identical in both themes.

#### Scenario: print output ignores the theme
- **WHEN** a document is printed under the dark palette
- **THEN** the physical output uses the document's own colors, not the theme's

#### Scenario: overlays identical across themes
- **WHEN** comparing the light and dark palettes
- **THEN** the text-selection and active search-match overlay colors are the same in both

### Requirement: Toolbar chrome is theme-owned

The toolbar strip and its controls SHALL render from the active palette on both platforms and SHALL NOT fall back to host-system chrome colors for surfaces the palette defines.

#### Scenario: dark toolbar rendered from palette
- **WHEN** the dark palette is active
- **THEN** the toolbar strip, button backgrounds, checked-state emphasis, glyphs, and edit fields derive from the dark palette on both platforms
