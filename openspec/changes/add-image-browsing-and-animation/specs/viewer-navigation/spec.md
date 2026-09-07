## MODIFIED Requirements

### Requirement: Clamp navigation at document bounds

The viewer SHALL NOT navigate beyond the first or last page. Issuing a navigation command that would move past either bound MUST leave the current page unchanged, except for a standalone raster image document where next at the last page or prev at the first page SHALL open the adjacent sibling image in the same directory when one exists.

#### Scenario: Next at last page
- **WHEN** the viewer is on the last page of a multi-page document and a "next page" command is issued
- **THEN** the viewer remains on the last page

#### Scenario: Previous at first page
- **WHEN** the viewer is on page 1 of a multi-page document and a "previous page" command is issued
- **THEN** the viewer remains on page 1

#### Scenario: Next at last page of a standalone image opens sibling
- **WHEN** the viewer is on the sole page of a standalone raster image document and a sibling image exists after it in natural order, and a "next page" command is issued
- **THEN** the viewer opens that sibling image

#### Scenario: Previous at first page of a standalone image opens sibling
- **WHEN** the viewer is on the sole page of a standalone raster image document and a sibling image exists before it in natural order, and a "previous page" command is issued
- **THEN** the viewer opens that sibling image