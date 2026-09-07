# Image Engine Specification

## Purpose

Lets the lister open standalone raster images (JPEG/PNG/GIF/BMP/ICO) through a dedicated Qt QImageReader-backed engine, treating every multi-frame file as a single document page. Only natively-decodable raster formats are routed here; TIFF is a document (see `tiff-document-support`) and WEBP has no decoder in the build.

## Requirements

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