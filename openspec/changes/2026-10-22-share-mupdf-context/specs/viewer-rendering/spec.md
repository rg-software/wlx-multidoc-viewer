# Viewer Rendering — delta (share-mupdf-context)

## Requirements

### Requirement: Open MuPDF-backed documents in concurrent windows

MuPDF-backed documents (PDF, XPS, EPUB, MOBI, FB2, CHM, CBZ, TIFF) SHALL open
in any number of concurrently open viewer windows, whether on the same file or
on different files. Opening a second such document while a first is still open
MUST NOT fail the open and MUST NOT disturb the already-open document's
rendering, text, search, or theming. ICC color management (see the embedded
profile requirement above) SHALL keep applying in every window.

#### Scenario: Same file in two windows

- **WHEN** the host loads the same PDF (or EPUB/MOBI/CHM/CBZ) file in two
  Lister windows at the same time
- **THEN** both windows display the document with identical page counts, text,
  and colors, and closing either window leaves the other fully functional

#### Scenario: Two different MuPDF documents at once

- **WHEN** two different MuPDF-backed files are open concurrently
- **THEN** each window renders, searches, and themes its own document; the
  CHM stylesheet never restyles a reflowable document in another window and
  vice versa

#### Scenario: CHM second window stages its own copy

- **WHEN** a second window opens a CHM file that is already open (libchm
  holds the first copy with an exclusive lock)
- **THEN** the engine stages a second private temp copy, both windows render
  independently, and closing a window deletes only its own copy
