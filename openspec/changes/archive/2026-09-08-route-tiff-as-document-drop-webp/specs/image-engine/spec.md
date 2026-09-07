## REMOVED Requirements

### Requirement: Open a standalone raster image (includes multi-frame TIFF)

**Reason**: The image engine's raster roster is narrowed to natively-decodable formats (`.jpg/.jpeg/.png/.gif/.bmp/.ico`). TIFF is no longer handled here — it is a document opened via the MuPDF engine (see `tiff-document-support`), so a multi-frame TIFF is a multi-page document, not one composite image page.

### Requirement: Fall back to MuPDF on decode failure

**Reason**: The raster roster is narrowed to natively-decodable formats, so every file that reaches `ImageEngine` is decodable and a MuPDF fallback is neither needed nor reachable; the image folder count is honest by construction.
**Migration**: TIFF opens through the MuPDF document engine (see `tiff-document-support`). WEBP is unsupported (no decoder in the build).

## ADDED Requirements

### Requirement: Open a standalone raster image

The viewer SHALL open a natively-decodable raster image file (`.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.ico`, case-insensitive) as a single-page document rendered through Qt's image decoding.

#### Scenario: Open a JPEG
- **WHEN** the user opens a `.jpg` file that Qt can decode
- **THEN** the viewer loads it as a one-page document and shows the decoded image

### Requirement: Single-page treatment of multi-frame files

A multi-frame GIF SHALL be presented as a single document page whose frames animate in place; frames MUST NOT be exposed as step-through pages.

#### Scenario: Multi-frame GIF is one page
- **WHEN** the user opens a `.gif` with multiple frames
- **THEN** the viewer reports a page count of 1 for the document

#### Scenario: Animated frame advances in place
- **WHEN** an animated frame advances
- **THEN** the current page does not change; only the displayed content of that page changes
