# Text Selection Specification

## Purpose

Lets users select text directly in the rendered document with the mouse and copy it to the clipboard, whenever the underlying document carries a machine-readable text layer — matching the interaction model users know from standalone document viewers.

## Requirements

### Requirement: Selectable-text detection

The viewer SHALL determine, per open document, whether pages expose a selectable text layer, and SHALL treat documents or pages without one (e.g., scans without OCR, photo archives) as non-selectable.

#### Scenario: Document with text layer
- **WHEN** a PDF containing embedded text is opened
- **THEN** text selection is available for its pages

#### Scenario: Document without text layer
- **WHEN** an image-only scan or a comic archive without text is opened
- **THEN** no selection affordances appear and left-drag behaves exactly as before this capability

### Requirement: Cursor feedback over selectable text

The viewer SHALL show the standard I-beam cursor while the pointer hovers over selectable text within a page, and the usual arrow/hand cursors everywhere else.

#### Scenario: Hovering over a word
- **WHEN** the pointer moves over glyphs of a selectable page
- **THEN** the cursor becomes I-beam

#### Scenario: Hovering outside text
- **WHEN** the pointer sits over a page margin, background, or a non-selectable page
- **THEN** the cursor returns to the default viewer cursor

### Requirement: Mouse range selection

The viewer SHALL start a selection when a left-button press begins on selectable text, extend it as the pointer drags by snapping each endpoint to the nearest text item boundary, render highlighted rectangles behind every selected text item (possibly spanning multiple lines and, in continuous mode, multiple pages), and finalize the selection on button release. Selected items on the same visual line SHALL be highlighted as one contiguous span, including the spaces between them.

#### Scenario: Drag across part of a line
- **WHEN** the user presses on a word and drags rightward along the same line
- **THEN** every text item between anchor and current pointer, including the spaces between them, is highlighted contiguously while dragging and remains highlighted after release

#### Scenario: Selection spanning lines and pages
- **WHEN** the drag crosses line breaks or continues into subsequent pages of the continuous strip
- **THEN** highlights follow the selected items across those lines and pages, one contiguous span per line

#### Scenario: Backwards selection
- **WHEN** the user drags from a later text position back to an earlier one
- **THEN** the same range is selected as if chosen front-to-back

### Requirement: Selection versus drag-pan precedence

A left-button press that does not begin on selectable text SHALL preserve the existing drag-pan behavior; only presses starting on selectable text begin a selection.

#### Scenario: Press on empty margin
- **WHEN** the user left-drags starting on page background or outside any page
- **THEN** the view pans as before and no selection is started

### Requirement: Copy selection to clipboard

The viewer SHALL copy the currently selected text to the system clipboard when the user issues copy (`Ctrl+C`), joining text items into lines and lines into paragraphs; with no active selection the shortcut SHALL be ignored. Both platforms SHALL use their native clipboard.

#### Scenario: Copy after selecting a sentence
- **WHEN** the user selects a sentence and presses `Ctrl+C`
- **THEN** the sentence's characters are placed on the clipboard as plain text

#### Scenario: Copy with nothing selected
- **WHEN** `Ctrl+C` is pressed while no selection exists
- **THEN** the clipboard is left unchanged

### Requirement: Selection lifetime

The viewer SHALL clear the visible selection and its copied state when the user presses `Esc`, clicks again without starting a new selection range, or the view changes via zoom, rotation, display-mode toggle, page change in paged mode, or document close/reload. Scrolling in continuous mode SHALL NOT clear an existing selection.

#### Scenario: Escape clears
- **WHEN** the user presses `Esc` with an active selection
- **THEN** all highlights disappear and subsequent `Ctrl+C` copies nothing

#### Scenario: Scroll keeps selection
- **WHEN** the user scrolls a continuous-mode document after making a selection
- **THEN** the selection remains and highlights stay anchored to the correct words when they scroll back into view