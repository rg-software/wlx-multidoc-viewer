## MODIFIED Requirements

### Requirement: Clamp navigation at document bounds

The viewer SHALL NOT navigate beyond the first or last page of a document, except when the open document is a standalone image file and a sibling image exists in the same folder (see `image-folder-browsing`); in that case a boundary command SHALL open the sibling. Absent an applicable sibling, issuing a navigation command that would move past either bound MUST leave the current page unchanged.

#### Scenario: Next at last page
- **WHEN** the viewer is on the last page and a "next page" command is issued, and the document is not a standalone image or has no following sibling
- **THEN** the viewer remains on the last page

#### Scenario: Previous at first page
- **WHEN** the viewer is on page 1 and a "previous page" command is issued, and the document is not a standalone image or has no preceding sibling
- **THEN** the viewer remains on page 1

#### Scenario: Next at last page of an image with a sibling
- **WHEN** the viewer is on the last page of a standalone image and a "next page" command is issued, and a following sibling exists
- **THEN** the viewer opens the following sibling as defined by `image-folder-browsing`

#### Scenario: Previous at first page of an image with a sibling
- **WHEN** the viewer is on page 1 of a standalone image and a "previous page" command is issued, and a preceding sibling exists
- **THEN** the viewer opens the preceding sibling as defined by `image-folder-browsing`