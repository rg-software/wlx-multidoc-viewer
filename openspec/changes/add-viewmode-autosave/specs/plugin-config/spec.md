# Spec Delta

## ADDED Requirements

### Requirement: Autosave view-mode flag key

The `[Viewer]` section SHALL support a boolean `AutosaveViewMode` key that turns on saving and restoring of the last-used view mode (see `view-mode-persistence`). Accepted values are case-insensitive: `1`, `true`, `yes`, `on` enable it; `0`, `false`, `no`, `off` disable it. When the key is absent or malformed, autosave SHALL be disabled. The key is read-only like every other INI key: the plugin MUST NOT write it back.

#### Scenario: Flag enabled
- **WHEN** `multidocviewer.ini` contains `[Viewer] AutosaveViewMode=true`
- **THEN** view-mode autosave is active for the process

#### Scenario: Flag absent
- **WHEN** `multidocviewer.ini` contains no `[Viewer] AutosaveViewMode` key
- **THEN** autosave is disabled and behavior matches a build without the feature

#### Scenario: Malformed flag value
- **WHEN** `[Viewer] AutosaveViewMode` contains an unparseable string (e.g. `maybe`)
- **THEN** autosave is disabled
