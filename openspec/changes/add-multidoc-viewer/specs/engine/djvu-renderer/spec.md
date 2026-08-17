## Purpose

Provides DjVu document loading, page rendering, and text extraction using DjVuLibre, exposed through the same engine interface as the MuPDF backend.

## ADDED Requirements

### Requirement: DjVu format support
The DjVu engine SHALL open and render `.djvu` and `.djv` files.

#### Scenario: Open DjVu document
- **WHEN** a `.djvu` file is passed to the DjVu engine
- **THEN** the engine SHALL open the document successfully and report its page count

#### Scenario: Open corrupted DjVu
- **WHEN** a truncated or corrupted `.djvu` file is passed to the DjVu engine
- **THEN** the engine SHALL return an error and SHALL NOT crash

### Requirement: DjVu page rendering
The DjVu engine SHALL render any page to a QImage in `Format_RGB888`.

#### Scenario: Render DjVu page at default zoom
- **WHEN** the engine renders page 1 at zoom factor 1.0
- **THEN** the output QImage SHALL contain the page content rendered at 96 DPI

#### Scenario: Render DjVu page at higher DPI
- **WHEN** the engine renders page 1 at zoom factor 2.0
- **THEN** the output QImage SHALL be twice the dimensions of the 1.0 rendering

#### Scenario: Render out-of-range DjVu page
- **WHEN** a page number less than 1 or greater than the page count is requested
- **THEN** the engine SHALL return an empty QImage

### Requirement: DjVu page count
The engine SHALL report the total number of pages.

#### Scenario: Multi-page DjVu
- **WHEN** a 15-page DjVu document is opened
- **THEN** `pageCount()` SHALL return 15

### Requirement: DjVu text extraction
The DjVu engine SHALL extract text from pages that contain an embedded text layer.

#### Scenario: Text layer present
- **WHEN** text extraction is requested for a DjVu page with OCR text
- **THEN** the engine SHALL return the text as a UTF-8 string

#### Scenario: No text layer
- **WHEN** text extraction is requested for a DjVu page without an OCR text layer
- **THEN** the engine SHALL return an empty string

### Requirement: DjVu page dimensions
The engine SHALL report the natural dimensions (width and height in pixels) of each page.

#### Scenario: Get page size
- **WHEN** page dimensions are queried for page 1 of a DjVu document
- **THEN** the engine SHALL return the page's width and height in pixels at the document's resolution

### Requirement: DjVu resource cleanup
The DjVu engine SHALL release all DjVuLibre handles when the document is closed.

#### Scenario: Close DjVu document
- **WHEN** a DjVu document is closed
- **THEN** the `ddjvu_document_t` and all `ddjvu_page_t` handles SHALL be destroyed and no DjVuLibre memory SHALL remain allocated
