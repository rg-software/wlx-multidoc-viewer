## ADDED Requirements

### Requirement: Sidebar background color configuration

The `[Viewer]` section SHALL support a `SidebarBackground` key specifying the outline sidebar's background as a hex RGB value (e.g. `#F0F0F0` or `F0F0F0`). The value is case-insensitive and the `#` prefix is optional. When the key is absent or malformed, the sidebar SHALL use the default `0xE8E8E8` (light gray). The sidebar setting SHALL NOT affect the page-area background, and the page-area `BackgroundColor` setting SHALL NOT affect the sidebar.

#### Scenario: Valid sidebar background color in INI
- **WHEN** `multidocviewer.ini` contains `[Viewer] SidebarBackground=#F0F0F0`
- **THEN** the outline sidebar renders with the `#F0F0F0` background on both platforms, and the page area keeps its own configured background

#### Scenario: Missing SidebarBackground key
- **WHEN** `multidocviewer.ini` is absent or contains no `[Viewer] SidebarBackground`
- **THEN** the sidebar renders with the default light-gray background (`0xE8E8E8`)

#### Scenario: Malformed SidebarBackground value
- **WHEN** `[Viewer] SidebarBackground` contains a non-hex string (e.g. `sidebar`)
- **THEN** the sidebar falls back to the default `0xE8E8E8`

#### Scenario: Sidebar setting leaves page background untouched
- **WHEN** `multidocviewer.ini` sets `[Viewer] SidebarBackground` to a color different from `BackgroundColor`
- **THEN** the page area and the sidebar each render with their own configured colors