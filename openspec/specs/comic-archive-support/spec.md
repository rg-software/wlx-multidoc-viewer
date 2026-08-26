## Purpose

Lets the lister open RAR-based (CBR) and 7-Zip-based (CB7) comic book archives as multi-page documents. Pages are the image entries inside the archive, ordered naturally; each page renders to a bitmap through Qt's image decoding.

## Requirements

### Requirement: Open a comic archive

The viewer SHALL accept a `.cbr` or `.cb7` file whose underlying container is a RAR or 7-Zip archive and open it as a multi-page document where each "page" is one image entry inside the archive.

#### Scenario: Open a RAR-based CBR
- **WHEN** the user opens a `.cbr` file that is a genuine RAR archive of images
- **THEN** the viewer loads it, exposes the image count as the page count, and shows page 1

#### Scenario: Open fails on corrupt container
- **WHEN** the user opens a `.cbr`/`.cb7` file that is corrupt or not a readable archive
- **THEN** the viewer does not crash and the lister shows no document

### Requirement: Page enumeration in natural order

The viewer SHALL treat archive entries whose name ends in `.png`, `.jpg`, `.jpeg`, `.gif`, or `.bmp` (case-insensitive) as pages, ordered so that embedded digit sequences compare numerically (natural order), making `page2` precede `page10`.

#### Scenario: Numeric ordering across decades
- **WHEN** the archive contains entries named `page1.jpg` … `page12.jpg`
- **THEN** the page order is numeric (`page2` before `page10`), not lexicographic

#### Scenario: Non-image entries ignored
- **WHEN** the archive mixes images with text files, cover.txt-style metadata, or nested directories
- **THEN** only image entries become pages

### Requirement: Render a comic page

The viewer SHALL decode the selected entry through Qt's image loading and render it honoring zoom, DPI scale, and rotation.

#### Scenario: Render honors display parameters
- **WHEN** the viewer paints a comic page with zoom and rotation applied
- **THEN** the bitmap dimensions reflect the zoom/DPI product and the rotation orientation
