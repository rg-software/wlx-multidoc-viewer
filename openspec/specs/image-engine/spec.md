# Image Engine Specification

## Purpose

Lets the lister open standalone raster images (JPEG/PNG/GIF/TIFF/BMP/WEBP) through a dedicated Qt QImageReader-backed engine instead of the MuPDF fallback, treating every multi-frame file as a single document page.

## Requirements

### Requirement: Open a standalone raster image

The viewer SHALL open a raster image file (`.jpg`, `.jpeg`, `.png`, `.gif`, `.tif`, `.tiff`, `.bmp`, `.webp`, case-insensitive) as a single-page document rendered through Qt's image decoding.

#### Scenario: Open a JPEG
- **WHEN** the user opens a `.jpg` file that Qt can decode
- **THEN** the viewer loads it as a one-page document and shows the decoded image

#### Scenario: Open a multi-frame TIFF as one page
- **WHEN** the user opens a `.tif` file containing multiple embedded frames
- **THEN** the viewer exposes it as exactly one page, not one page per frame

### Requirement: Single-page treatment of multi-frame files

A multi-frame GIF or TIFF SHALL be presented as a single document page whose frames animate in place; frames MUST NOT be exposed as step-through pages.

#### Scenario: Multi-frame GIF is one page
- **WHEN** the user opens a `.gif` with multiple frames
- **THEN** the viewer reports a page count of 1 for the document

#### Scenario: Animated frame advances in place
- **WHEN** an animated frame advances
- **THEN** the current page does not change; only the displayed content of that page changes

### Requirement: Fall back to MuPDF on decode failure

When the image engine cannot decode a raster file, the viewer SHALL attempt to open it through the prior MuPDF path so that no currently-openable file regresses.

#### Scenario: Undecodable file falls back
- **WHEN** the user opens a raster-suffixed file that the image engine fails to decode but MuPDF opens
- **THEN** the viewer shows the document via the fallback path instead of showing no document

#### Scenario: Neither engine succeeds
- **WHEN** the user opens a file that neither the image engine nor MuPDF can decode
- **THEN** the lister shows no document and the viewer does not crash