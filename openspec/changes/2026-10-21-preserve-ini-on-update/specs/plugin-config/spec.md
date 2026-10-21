# Spec Delta

## Purpose

Define how the plugin's default configuration is shipped so that installing or updating the plugin never overwrites the user's `multidocviewer.ini`.

## ADDED Requirements

### Requirement: Ship configuration as template

Release and install packaging SHALL ship the default configuration as `multidocviewer.ini.sample`, never as a live `multidocviewer.ini`.

#### Scenario: Package contents

- **WHEN** a release archive or install image is produced (Windows or Linux)
- **THEN** it contains `multidocviewer.ini.sample` with the documented defaults and does not contain `multidocviewer.ini`

### Requirement: Preserve existing configuration on update

The system SHALL NOT overwrite an existing `multidocviewer.ini` when a newer version of the plugin is installed.

#### Scenario: Update with existing INI

- **WHEN** the plugin is installed or updated into a directory that already contains `multidocviewer.ini`
- **THEN** the existing `multidocviewer.ini` remains unchanged (the package carries only the `.sample`)

### Requirement: Absent INI keeps working

A missing `multidocviewer.ini` SHALL continue to mean compile-time defaults; the viewer SHALL NOT require the user to create the file.

#### Scenario: Fresh install without customization

- **WHEN** only `multidocviewer.ini.sample` is present and no `multidocviewer.ini` exists
- **THEN** the viewer behaves exactly as with an absent INI (all defaults)
