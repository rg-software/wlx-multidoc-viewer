## Purpose

Opens MOBI/PRC ebook files in the lister as multi-page documents, surfacing the embedded cover image (from the MOBI EXTH header) as page 1 and falling back to plain MuPDF behavior when no cover exists.

## ADDED Requirements

### Requirement: Open a MOBI/PRC ebook

The viewer SHALL accept a `.mobi` or `.prc` file and open it as a multi-page document whose text content is laid out and rendered through the MuPDF HTML pipeline.

#### Scenario: Open a valid MOBI
- **WHEN** the user opens a valid `.mobi`/`.prc` file
- **THEN** the viewer loads the ebook, exposes the page count, and shows page 1

#### Scenario: Open a MOBI with no readable content
- **WHEN** the user opens a `.mobi`/`.prc` file that is corrupt or has no parseable text records
- **THEN** the viewer does not crash and the lister shows no document

### Requirement: Surface the cover as page 1

When a MOBI declares a cover image in its EXTH header (type 201) and that image is present in the file, the viewer SHALL render the cover as page 1, ahead of the ebook's text body. The cover must be a real page that participates in all standard viewer features: page count, navigation, zoom, fit, rotation, selection, search, and outline.

#### Scenario: Cover present
- **WHEN** a MOBI's EXTH metadata references an image record and that record holds a recognizable image
- **THEN** the cover renders as page 1 and the ebook body begins on page 2

#### Scenario: Cover participates in navigation
- **WHEN** the user opens a MOBI with a cover and issues a next-page command from page 1
- **THEN** the viewer moves to the first text page (page 2), and first/last/jump commands account for the cover like any other page

#### Scenario: Cover renders with standard display features
- **WHEN** the user is on the MOBI cover page and applies zoom, fit, rotation, or switches display mode
- **THEN** the cover is rendered and transformed like any other page, with no viewer-side special casing

### Requirement: Fall back when no cover exists

When a MOBI has no usable cover — no EXTH[201] entry, or the referenced record is not a recognizable image — the viewer SHALL open the ebook without a cover, behaving exactly as a MOBI with no cover metadata.

#### Scenario: No EXTH cover reference
- **WHEN** a MOBI has no EXTH type-201 entry
- **THEN** the viewer renders the ebook body only, with the first text page as page 1

#### Scenario: Cover record is not an image
- **WHEN** a MOBI's EXTH[201] references a record that does not decode as an image
- **THEN** the viewer renders the ebook body only and does not fail to open
