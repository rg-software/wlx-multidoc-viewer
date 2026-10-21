# Spec Delta

## MODIFIED Requirements

### Requirement: Default mode is continuous

The viewer SHALL start in continuous mode with single-page presentation when a document is opened, unless view-mode autosave is enabled and a saved view-mode record exists (see `view-mode-persistence`), in which case the saved display mode and page presentation are restored instead.

#### Scenario: Fresh load
- **WHEN** a document is opened with autosave disabled, or with autosave enabled but no saved record
- **THEN** the viewer is in continuous mode on page 1 with single-page presentation

#### Scenario: Autosaved mode restored
- **WHEN** a document is opened with autosave enabled and the saved record says paged mode with double page presentation
- **THEN** the viewer is in paged mode with double page presentation on page 1
