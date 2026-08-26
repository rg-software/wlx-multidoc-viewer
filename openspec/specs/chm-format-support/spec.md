## Purpose

Lets the lister open Microsoft Compiled HTML Help (`.chm`) files and display their pages through the existing viewer surface. Pages are the `.htm` / `.html` entries of the CHM archive in reading order (home topic first, then `.hhc` table-of-contents order); the outline mirrors the nested `.hhc` table of contents, and each page exposes a selectable, searchable text layer through MuPDF.

## Requirements

### Requirement: Open a CHM archive

The viewer SHALL accept a `.chm` file as input and open it as a multi-page document, where each "page" is one HTML entry inside the CHM archive.

#### Scenario: Open succeeds
- **WHEN** the user opens a valid `.chm` file
- **THEN** the viewer loads the CHM archive, exposes the page count from the HTML entries, and shows page 1

#### Scenario: Open fails on corrupt file
- **WHEN** the user opens a file with the `.chm` extension but the underlying archive is corrupt or not a CHM
- **THEN** the viewer does not crash and the lister shows no document

### Requirement: Page enumeration from CHM archive

The viewer SHALL enumerate CHM pages in reading order rather than raw archive order: the default topic (home) first when it resolves to an HTML entry, followed by the topics listed in the `.hhc` table-of-contents file in their listed order, followed by any remaining `.htm` / `.html` entries in archive order. Each HTML entry SHALL appear exactly once in the resulting page list.

#### Scenario: Home topic shown on open
- **WHEN** the user opens a valid `.chm` whose default topic resolves to an HTML entry
- **THEN** that entry is page 1 and the viewer shows it on open

#### Scenario: Table-of-contents order preserved
- **WHEN** the `.hhc` file lists several topics in a specific order
- **THEN** those topics appear as consecutive pages following the home topic, in `.hhc` order

#### Scenario: Entries outside the TOC remain reachable
- **WHEN** the archive contains HTML entries not referenced by the `.hhc`
- **THEN** they are appended after the TOC topics, in archive order, so every HTML entry maps to exactly one page

#### Scenario: No ordering source available
- **WHEN** the CHM has neither a resolvable default topic nor a parseable `.hhc`
- **THEN** pages fall back to plain archive order and the viewer behaves normally

### Requirement: Render a CHM page

The viewer SHALL render the selected CHM page to a bitmap suitable for the lister's paint path, sized by the requested zoom and DPI.

#### Scenario: Render a page
- **WHEN** the user is on a CHM page and the viewer paints
- **THEN** the viewer renders that page's HTML content to the bitmap, honoring zoom and DPI

#### Scenario: HTML with relative links
- **WHEN** the CHM page contains relative links to other HTML entries in the same archive
- **THEN** the rendered bitmap at minimum renders the visible HTML of the page; relative links need not be live in v1

### Requirement: Navigate pages

The viewer SHALL support the same page navigation (next, previous, first, last, jump to page) for CHM as for any other supported format. Bounds apply: next on the last page is a no-op, previous on page 1 is a no-op.

#### Scenario: Next on last page is a no-op
- **WHEN** the viewer is on the last CHM page and the user issues a next-page command
- **THEN** the viewer remains on the last page

#### Scenario: Jump to a valid page
- **WHEN** the viewer receives a target page index within `[1, pageCount]`
- **THEN** the viewer displays that page

### Requirement: Outline from the CHM table of contents

The viewer SHALL build the outline from the CHM's `.hhc` table-of-contents file, preserving `<ul>` nesting as nested outline items, where each item's destination is the page index of its resolved topic path (or page 1 when the path does not resolve). When `.hhc` is missing or unparseable, the viewer SHALL fall back to the flat `/#WINDOWS` outline; when both are unavailable the outline SHALL be empty.

#### Scenario: Nested TOC reflected
- **WHEN** the `.hhc` contains top-level sections with nested sub-topics
- **THEN** the outline shows the same hierarchy, and activating an item navigates to that topic's page

#### Scenario: Topic path does not resolve
- **WHEN** a `.hhc` item references a path that is missing or not an HTML entry
- **THEN** the item still appears in the outline with destination page 1

#### Scenario: Fallback without `.hhc`
- **WHEN** the CHM has no `.hhc` entry
- **THEN** the outline falls back to the flat `/#WINDOWS` rows and navigation works normally

### Requirement: Codepage handling (v1 limited)

The viewer SHALL read `/#SYSTEM` to determine the CHM's codepage. In v1, codepage 1252 (Latin-1, the default) and `CP_ACP` are fully supported; other codepages render the bytes as-is and the user may see mojibake for non-ASCII paths.

#### Scenario: Codepage 1252
- **WHEN** the CHM declares codepage 1252 in `/#SYSTEM`
- **THEN** HTML entries with non-ASCII content render with Latin-1 characters

#### Scenario: Unknown codepage
- **WHEN** the CHM declares a codepage other than 1252 / ACP
- **THEN** the viewer renders the HTML but may show mis-encoded characters; the rest of the viewer (page count, navigation) still works

### Requirement: Detect CHM via WLX detect string

The viewer SHALL register `EXT="CHM"` in the WLX detect string so that Total Commander / Double Commander offer the lister for `.chm` files.

#### Scenario: WLX detect string includes CHM
- **WHEN** the lister is installed and the user opens a `.chm` file from the file panel
- **THEN** the lister is offered by the host

### Requirement: CHM text layer and search

The engine SHALL expose a structured text layer for CHM pages (words with page-space bounding boxes) and positional whole-document search over them, so the viewer enables text selection, copy, and the find controls for CHM as it does for MuPDF-backed formats.

#### Scenario: Find locates a term across pages
- **WHEN** the user searches for a term that occurs on several CHM pages
- **THEN** matches are reported per page with normalized highlight rectangles anchored to the glyphs

#### Scenario: Match-case honored
- **WHEN** the user runs a search with match-case enabled
- **THEN** hits differing only in letter case are not reported

#### Scenario: Selection yields text
- **WHEN** the user selects a region of a rendered CHM page and copies
- **THEN** the copied text corresponds to the words under the selection
