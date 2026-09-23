## Purpose

Makes DjVu documents usable immediately on open by reporting the page count from the document directory and measuring page dimensions on demand, so open time does not scale with page count.

## ADDED Requirements

### Requirement: DjVu page count from the document directory

The DjVu engine SHALL report the document's page count from its directory, exactly, as soon as the document opens, without measuring or decoding any page.

#### Scenario: Multi-page document opens
- **WHEN** a multi-page DjVu document is opened
- **THEN** the engine reports the full page count before any page dimensions are requested

#### Scenario: Page count independent of page content
- **WHEN** the document's directory is intact
- **THEN** the reported page count equals the number of pages in the directory, regardless of whether their content has been read

### Requirement: Demand-driven page dimension measurement

The DjVu engine SHALL measure a page's dimensions only when requested, SHALL cache the measured result for the life of the open document, and MUST NOT require a full-page pixel decode to obtain dimensions.

#### Scenario: Repeated dimension queries
- **WHEN** the same page's dimensions are requested more than once
- **THEN** the engine returns the cached result without re-measuring

#### Scenario: Measuring a page does not render it
- **WHEN** dimensions are requested for a page that has never been rendered
- **THEN** the engine returns the page's dimensions without producing a rendered bitmap

### Requirement: Open time independent of page count

Opening a DjVu document SHALL NOT measure every page's dimensions before the document becomes usable; the number of pages measured during open SHALL be bounded by the current view unit.

#### Scenario: Large document opens quickly
- **WHEN** a DjVu document with many pages is opened
- **THEN** the viewer displays the first page without first measuring the dimensions of every page
