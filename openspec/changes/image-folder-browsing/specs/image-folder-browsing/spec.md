## Purpose

Lets users flip through the images in a folder from inside the lister instead of closing and reopening the lister for each adjacent file. The viewer treats a standalone image file as one item in a folder sequence and spans to the neighboring file at document boundaries. Applies to both the Win32 viewer on Windows and the Qt viewer on Linux.

## ADDED Requirements

### Requirement: Discover sibling images

When a plugin-supported standalone image file is opened, the viewer SHALL scan the file's directory for other image files supported by the plugin and record the current file's position, so the adjacent items in the sequence are available for navigation. The scan SHALL run once per opened file on both platforms.

#### Scenario: Folder with multiple image files
- **WHEN** a JPEG file is opened in a folder that also contains PNG and GIF files supported by the plugin
- **THEN** the viewer records the JPEG's position among all supported image files in the folder

#### Scenario: Folder scan failure
- **WHEN** the directory cannot be read due to a permission or I/O error
- **THEN** the viewer behaves as if no sibling images exist and navigation remains clamped at the document bounds

### Requirement: Order sibling images naturally

The sibling sequence SHALL be ordered case-insensitively using human/natural ordering (for example `img2.png` before `img10.png`), consistent with the ordering already applied to comic archive entries.

#### Scenario: Natural ordering of mixed names
- **WHEN** the folder contains `img2.png`, `img10.png`, and `img1.png`
- **THEN** navigation visits `img1.png`, then `img2.png`, then `img10.png`

### Requirement: Span to the next and previous sibling at document bounds

For a standalone image file, the folder forms a continuous page sequence: the last page of one file is followed by the first page of the next file, and the first page of a file is preceded by the last page of the previous file. Issuing a "next page" command on the last rendered page SHALL open the following sibling and display its first page when such a sibling exists. Issuing a "previous page" command on the first rendered page SHALL open the preceding sibling and display its last page when such a sibling exists. For a single-frame image, the first and last rendered pages are the same page. When no sibling exists in the requested direction, the command SHALL leave the current document and page unchanged.

#### Scenario: Next spans to the following sibling
- **WHEN** an image file is open on its last rendered page, a next-page command is issued, and a following sibling exists
- **THEN** the viewer opens the following sibling and displays its first page

#### Scenario: Previous spans to the preceding sibling
- **WHEN** an image file is open on its first rendered page, a previous-page command is issued, and a preceding sibling exists
- **THEN** the viewer opens the preceding sibling and displays its last page

#### Scenario: No next sibling at the boundary
- **WHEN** an image file is open on its last rendered page, a next-page command is issued, and no following sibling exists
- **THEN** the viewer remains on the same page of the same file

### Requirement: Enable toolbar buttons at boundaries when siblings exist

The Previous and Next toolbar buttons SHALL remain enabled at the first and last rendered page respectively when a sibling image exists in the corresponding direction, and SHALL be disabled when none exists.

#### Scenario: First page with a preceding sibling
- **WHEN** an image file is open on its first rendered page and a preceding sibling exists
- **THEN** the Previous button is enabled

#### Scenario: Last page with no following sibling
- **WHEN** an image file is open on its last rendered page and no following sibling exists
- **THEN** the Next button is disabled

### Requirement: File switch behaves like opening a new document

When a sibling image is opened through boundary navigation, the viewer SHALL clear the render cache, reset the scroll position, and reapply the fit mode to the new document as when a new file is loaded into the lister pane. The page indicator SHALL reflect the newly opened file and the landing page (first page for a next command, last page for a previous command).

#### Scenario: Switching to the next sibling
- **WHEN** the viewer opens the next sibling through boundary navigation
- **THEN** the page indicator shows the new file at page 1, in-place without closing the lister window

#### Scenario: Switching to the previous sibling
- **WHEN** the viewer opens the preceding sibling through boundary navigation
- **THEN** the page indicator shows the new file at its last page, and the viewport scroll position resets as for any newly opened document