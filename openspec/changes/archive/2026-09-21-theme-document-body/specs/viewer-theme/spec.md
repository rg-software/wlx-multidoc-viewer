## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Document and overlay rendering unaffected by theme

The rendered document bitmaps SHALL be themed per the `Document body theming` requirement. The print output SHALL NOT change with the theme. The text-selection and search-match overlay colors SHALL be identical in both themes.

#### Scenario: print output ignores the theme
- **WHEN** a document is printed under the dark palette
- **THEN** the physical output uses the document's own colors, not the theme's

#### Scenario: overlays identical across themes
- **WHEN** comparing the light and dark palettes
- **THEN** the text-selection and active search-match overlay colors are the same in both
