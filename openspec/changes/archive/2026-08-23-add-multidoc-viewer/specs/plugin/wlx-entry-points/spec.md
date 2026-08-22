## Purpose

Implements the Double Commander / Total Commander WLX plugin interface so the multi-document viewer is discoverable and loadable by the file manager's Quick View panel and lister.

## ADDED Requirements

### Requirement: Plugin entry points
The plugin SHALL export the four mandatory WLX C functions: `ListLoad`, `ListLoadNext`, `ListCloseWindow`, and `ListGetDetectString`.

#### Scenario: ListLoad creates viewer widget
- **WHEN** Double Commander calls `ListLoad(ParentWin, FileToLoad, ShowFlags)` with a file whose extension matches the detect string
- **THEN** the plugin SHALL return a valid HWND (widget handle) that is a child of `ParentWin` and displays the first page of the document

#### Scenario: ListLoad rejects unsupported file
- **WHEN** Double Commander calls `ListLoad` with a file whose extension does not match any supported format
- **THEN** the plugin SHALL return NULL (0) so Double Commander tries the next plugin

#### Scenario: ListLoadNext replaces document
- **WHEN** Double Commander calls `ListLoadNext(ParentWin, PluginWin, FileToLoad, ShowFlags)` on an already-open viewer
- **THEN** the plugin SHALL close the current document, load the new file into the same widget, display the first page, and return `LISTPLUGIN_OK`

#### Scenario: ListLoadNext fails on bad file
- **WHEN** Double Commander calls `ListLoadNext` and the new file cannot be opened
- **THEN** the plugin SHALL return `LISTPLUGIN_ERROR` and retain the previous document if still open

#### Scenario: ListCloseWindow cleans up
- **WHEN** Double Commander calls `ListCloseWindow(ListWin)`
- **THEN** the plugin SHALL release all resources (document handles, rendering contexts, Qt widgets) and free the widget

### Requirement: File type detection
The plugin SHALL declare a detect string that covers all supported file extensions.

#### Scenario: Detect string format
- **WHEN** Double Commander calls `ListGetDetectString(DetectString, maxlen)`
- **THEN** the plugin SHALL write a valid detect string of the form `EXT="PDF";EXT="DJVU";EXT="EPUB";...` (and so on for every supported extension) into the output buffer, not exceeding `maxlen`

#### Scenario: Case-insensitive matching
- **WHEN** a file has extension `.PDF` or `.DjVu` (any case)
- **THEN** the plugin SHALL still load it successfully

### Requirement: Multiple simultaneous instances
The plugin SHALL support multiple open viewer windows concurrently without sharing mutable state between them.

#### Scenario: Two files previewed at once
- **WHEN** Double Commander opens two preview panes (e.g. left and right panels) each loading a different file
- **THEN** each viewer instance SHALL operate independently with its own document handle, page state, and widget

### Requirement: Thread safety
All MuPDF context operations SHALL be confined to a single thread per document instance. The plugin SHALL NOT share a single `fz_context` across multiple document instances.

#### Scenario: Concurrent loading
- **WHEN** two files are loaded in rapid succession (e.g. user scrolls quickly in Quick View)
- **THEN** each load SHALL use its own `fz_context` and SHALL NOT corrupt the other's state
