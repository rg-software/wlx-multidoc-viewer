## Purpose

Treats TIFF as a document opened through the MuPDF engine (like PDF and CBR/CB7), rather than as a single-page raster image. A multi-page TIFF becomes a paged document (one page per IFD), and TIFF is kept out of the image folder's sibling roster. WEBP is not supported by any decoder in this build and is out of scope.

## ADDED Requirements

### Requirement: Open TIFF as a document

The viewer SHALL route a `.tif`/`.tiff` file to the MuPDF document engine, exposing its embedded images as pages.

#### Scenario: Single-page TIFF opens as a one-page document
- **WHEN** the user opens a `.tiff` that contains a single image
- **THEN** the viewer opens it via MuPDF as a one-page document

#### Scenario: Multi-page TIFF opens as a multi-page document
- **WHEN** the user opens a `.tiff` that contains multiple images (IFDs)
- **THEN** the viewer opens it via MuPDF as a multi-page document whose page count matches the embedded images

### Requirement: TIFF is not a folder-browsed image

TIFF SHALL NOT be listed in a directory's sibling-image set or counted in the image-folder total; it is reached by opening it directly, as with any other document format.

#### Scenario: Not counted as a sibling
- **WHEN** a directory contains `.jpg`, `.png`, and `.tiff` files and the viewer opens the `.jpg`
- **THEN** the sibling image count includes the `.png` but not the `.tiff`