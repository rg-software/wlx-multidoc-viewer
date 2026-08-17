## Purpose

Provides document loading, page rendering, text extraction, and metadata access for all MuPDF-supported formats through a unified engine interface.

## ADDED Requirements

### Requirement: Supported formats
The MuPDF engine SHALL open and render documents in the following formats: PDF, XPS/OXPS, EPUB, MOBI, FB2/FB2Z, CBZ, CBR, CB7, CBT, HTML, Markdown, plain text, and image files (JPEG, PNG, TIFF, GIF, BMP, WebP, AVIF, JXL, TGA, PSD).

#### Scenario: Open PDF
- **WHEN** a `.pdf` file is passed to the MuPDF engine
- **THEN** the engine SHALL open the document and report its page count

#### Scenario: Open EPUB
- **WHEN** a `.epub` file is passed to the MuPDF engine
- **THEN** the engine SHALL open the document, lay out pages, and report a page count

#### Scenario: Open JPEG image
- **WHEN** a `.jpg` file is passed to the MuPDF engine
- **THEN** the engine SHALL open it as a single-page document

#### Scenario: Open unsupported extension
- **WHEN** a file with an unrecognized extension is passed to the MuPDF engine
- **THEN** the engine SHALL attempt to open it (MuPDF sniffs content), and return an error if it fails

### Requirement: Page rendering
The engine SHALL render any page to a QImage in `Format_RGB888` (24-bit RGB, 8 bits per channel).

#### Scenario: Render at default zoom
- **WHEN** the engine is asked to render page 1 at zoom factor 1.0
- **THEN** the output QImage SHALL have dimensions matching the page's media box scaled by the zoom factor (72 DPI base)

#### Scenario: Render at specified zoom
- **WHEN** the engine is asked to render page 1 at zoom factor 2.0
- **THEN** the output QImage SHALL be twice the dimensions of the 1.0 zoom rendering

#### Scenario: Render out-of-range page
- **WHEN** the engine is asked to render a page number less than 1 or greater than the page count
- **THEN** the engine SHALL return an empty/null QImage and SHALL NOT crash

### Requirement: Page count
The engine SHALL report the total number of pages in a document.

#### Scenario: Multi-page document
- **WHEN** a 42-page PDF is opened
- **THEN** `pageCount()` SHALL return 42

#### Scenario: Single-page image
- **WHEN** a JPEG file is opened
- **THEN** `pageCount()` SHALL return 1

### Requirement: Text extraction
The engine SHALL extract plain text from a page when the format supports embedded text.

#### Scenario: Extract text from PDF
- **WHEN** text extraction is requested for a PDF page containing selectable text
- **THEN** the engine SHALL return the text content as a UTF-8 string

#### Scenario: Extract text from image
- **WHEN** text extraction is requested for a JPEG page
- **THEN** the engine SHALL return an empty string (images have no embedded text)

### Requirement: Document metadata
The engine SHALL expose document metadata fields: title, author, subject, creator, producer, creation date, and modification date.

#### Scenario: Read PDF metadata
- **WHEN** metadata is queried for a PDF with title "Annual Report" and author "Alice"
- **THEN** `metadata("title")` SHALL return "Annual Report" and `metadata("author")` SHALL return "Alice"

#### Scenario: Missing metadata
- **WHEN** metadata is queried for a field that is not set in the document
- **THEN** the engine SHALL return an empty string

### Requirement: Outline / table of contents
The engine SHALL read the document outline (table of contents) when available.

#### Scenario: PDF with bookmarks
- **WHEN** a PDF with a bookmarks outline is opened
- **THEN** the engine SHALL return a tree of outline items, each with a title and target page number

#### Scenario: Document without outline
- **WHEN** a document has no outline
- **THEN** the engine SHALL return an empty outline

### Requirement: Resource cleanup
The engine SHALL release all MuPDF resources (context, document, page, pixmap) when the document is closed.

#### Scenario: Close document
- **WHEN** a document is closed
- **THEN** all `fz_context`, `fz_document`, `fz_page`, and `fz_pixmap` handles SHALL be dropped, and no MuPDF memory SHALL remain allocated
