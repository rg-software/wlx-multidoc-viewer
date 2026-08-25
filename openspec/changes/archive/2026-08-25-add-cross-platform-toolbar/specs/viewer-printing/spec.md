# viewer-printing delta

## Purpose

Lets users print the open document through the platform's native print dialog, so printer, page range, copies, and other options are chosen in the familiar system UI while the plugin spools freshly rendered pages.

## ADDED Requirements

### Requirement: System print dialog invocation
Activating the toolbar print control SHALL open the platform-native print dialog pre-populated with the document's page range. Cancelling the dialog SHALL abort printing with no side effects on the viewer state.

#### Scenario: Cancel leaves state untouched
- **WHEN** the user opens the print dialog and cancels it
- **THEN** nothing is sent to any printer and the viewer continues showing the document unchanged

### Requirement: Print dialog drives output
The print job SHALL honor the settings chosen in the system dialog: selected printer, page range, number of copies, and orientation-related options where the dialog exposes them. Pages outside a restricted range MUST NOT be printed.

#### Scenario: Page range honored
- **WHEN** the user selects pages 3–5 and confirms
- **THEN** exactly those three pages are submitted to the chosen printer

#### Scenario: Copies honored
- **WHEN** the user requests two copies
- **THEN** the printer receives two copies of each selected page as directed by the dialog settings

### Requirement: Printer-resolution rendering
Printed pages SHALL be re-rendered for the target printer at the printer's resolution rather than scaled up from screen bitmaps, so text prints sharply. The current rotation and zoom-independent page geometry SHALL be preserved; each page SHALL be scaled to fit the printable paper area without cropping.

#### Scenario: Sharp text output
- **WHEN** a text-heavy PDF is printed on a 600 dpi printer
- **THEN** glyphs are rendered at printer resolution and print sharp, not as enlarged screen pixels

#### Scenario: On-screen rotation preserved
- **WHEN** the view is rotated 90° and the user prints
- **THEN** printed pages appear rotated the same way, fitted to the paper

### Requirement: Printing failure feedback
If printing cannot proceed (no printer installed, dialog reports an error, or the spooling fails), the viewer SHALL show an error indication and continue operating normally afterwards.

#### Scenario: No printer available
- **WHEN** the system has no configured printer and the user activates print
- **THEN** the viewer shows an error indication and no crash or hang occurs

### Requirement: Viewer remains usable after printing
After a print job has been submitted (or failed), the viewer SHALL return to its prior display state — same page, scroll position, mode, and highlights — and remain interactive while or after the job is processed.

#### Scenario: State restored after submit
- **WHEN** the user prints and returns to the viewer
- **THEN** the viewer shows the same page, scroll position, mode, and active search highlights as before printing
