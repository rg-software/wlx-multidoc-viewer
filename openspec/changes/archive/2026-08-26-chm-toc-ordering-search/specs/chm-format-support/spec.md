## MODIFIED Requirements

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

## ADDED Requirements

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
