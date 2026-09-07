## MODIFIED Requirements

### Requirement: Detect sibling images at load

When the viewer opens a standalone raster image, it SHALL find the other natively-decodable raster images in the same directory so the image can be navigated as part of its neighborhood. The sibling roster covers only the formats the image engine decodes directly (`.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.ico`); TIFF is a document and WEBP has no decoder, so neither is counted as a sibling image.

#### Scenario: Siblings discovered on open
- **WHEN** the user opens a raster image in a directory containing other natively-decodable raster images
- **THEN** the viewer records those sibling images for boundary navigation

#### Scenario: No siblings yields empty neighborhood
- **WHEN** the user opens a raster image in a directory with no other natively-decodable raster images
- **THEN** the viewer records an empty sibling set and boundary navigation is unavailable

#### Scenario: TIFF and WEBP are not counted as siblings
- **WHEN** a directory mixes natively-decodable images (`.jpg`/`.png`/`.gif`/`.bmp`/`.ico`) with TIFF or WEBP files
- **THEN** only the natively-decodable images are recorded; the TIFF/WEBP files are excluded from the sibling set and the image count

### Requirement: Deterministic natural ordering

The viewer SHALL order sibling images using natural (numeric-aware) comparison of their filenames, shared with comic archives, so that adjacent siblings are deterministic.

#### Scenario: Numeric sibling order
- **WHEN** a directory contains `img2.png` and `img10.png` on either side of the open image
- **THEN** `img2` precedes `img10` in sibling order regardless of lexicographic position