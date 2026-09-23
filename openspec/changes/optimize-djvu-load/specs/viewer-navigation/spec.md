## ADDED Requirements

### Requirement: Page count and direct jump available at open

The viewer SHALL report the open document's page count and SHALL accept a direct page jump as soon as the document has opened, without waiting for page-dimension measurement or the first render of every page to complete. The page count MUST be derived from the document's own structure and MUST be exact at open.

#### Scenario: Jump immediately after open
- **WHEN** a multi-page document has just opened and the user requests a direct jump to a valid page
- **THEN** the viewer navigates to that page

#### Scenario: Page count independent of measurement
- **WHEN** a document's pages have not all been measured
- **THEN** the viewer still reports the document's full page count and accepts jumps across the entire range
