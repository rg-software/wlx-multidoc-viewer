## Purpose

Lets the lister open Microsoft Compiled HTML Help (`.chm`) files and display their pages through the existing viewer surface. Pages are the `.htm` / `.html` entries of the CHM archive, ordered as the archive enumerates them; outline comes from the archive's `/#WINDOWS` table.

## ADDED Requirements

### Requirement: Open a CHM archive

The viewer SHALL accept a `.chm` file as input and open it as a multi-page document, where each "page" is one HTML entry inside the CHM archive.

#### Scenario: Open succeeds
- **WHEN** the user opens a valid `.chm` file
- **THEN** the viewer loads the CHM archive, exposes the page count from the HTML entries, and shows page 1

#### Scenario: Open fails on corrupt file
- **WHEN** the user opens a file with the `.chm` extension but the underlying archive is corrupt or not a CHM
- **THEN** the viewer does not crash and the lister shows no document

### Requirement: Page enumeration from CHM archive

The viewer SHALL enumerate pages by iterating the CHM archive entries in archive order and treating each entry whose path ends in `.htm` or `.html` as one page.

#### Scenario: Mixed archive contents
- **WHEN** the CHM archive contains HTML entries alongside binaries, images, and directory entries
- **THEN** only the `.htm` / `.html` entries are exposed as pages, in the archive's enumeration order

#### Scenario: Empty archive
- **WHEN** the CHM archive has zero HTML entries
- **THEN** the open succeeds but the page count is 0 and the user sees an empty document indicator

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

### Requirement: Outline from `/#WINDOWS`

The viewer SHALL expose a v1 outline built from the CHM's `/#WINDOWS` binary control file: parse the table at the byte offsets documented by nongnu.org/chmspec, look up the resulting path strings in `/#STRINGS`, and present each as an outline item whose destination is the corresponding HTML page index (when the path resolves to a page) or page 1 (when it does not).

#### Scenario: Outline with valid HTML destinations
- **WHEN** the CHM's `/#WINDOWS` references HTML entries that exist in the archive
- **THEN** the outline items have destination page indices that match the HTML pages the user navigates to

#### Scenario: Outline with non-HTML or missing destinations
- **WHEN** the CHM's `/#WINDOWS` references a path that is not an HTML entry or does not exist
- **THEN** the outline item still appears, with destination page 1

#### Scenario: Missing `/#WINDOWS`
- **WHEN** the CHM has no `/#WINDOWS` entry
- **THEN** the outline is empty and the rest of the viewer behaves normally

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
