## ADDED Requirements

### Requirement: Render embedded ICC color profiles

MuPDF-backed pages SHALL be converted to the display RGB space honoring any embedded ICC color profile attached to page content (raster images, spot-color overprints) and PDF device colorspaces (`DeviceCMYK`), instead of a naive component-wise fallback. When a document's content carries no color profile, conversion SHALL use the default CMYK→RGB behavior of the rendering engine.

#### Scenario: CMYK title image renders with its source colors
- **WHEN** the viewer opens a PDF whose first page is a CMYK JPEG with an embedded color profile (e.g. `examples/AC3_GW_Notebook_GER.pdf`)
- **THEN** the page shows the image's true source colors (dark brown backdrop with a lighter beige geometric pattern) rather than a near-black render with the pattern invisible

#### Scenario: Gray-only document unaffected
- **WHEN** the viewer opens a document whose pages are plain black/white or grayscale with no embedded color profiles
- **THEN** the render is unchanged in appearance, with no color shift or tint

#### Scenario: ICC-tagged RGB image
- **WHEN** the viewer renders a page containing an ICC-profile-tagged RGB image whose profile differs from the sRGB display assumption
- **THEN** the image colors are corrected through the profile so displayed colors match the source intent

#### Scenario: JPEG without embedded profile
- **WHEN** the viewer renders a JPEG that carries no ICC profile
- **THEN** the image converts with the engine default and shows no color shift compared to prior renders