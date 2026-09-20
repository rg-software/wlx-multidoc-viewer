## ADDED Requirements

### Requirement: Theme selection key

The `[Viewer]` section SHALL support a `Theme` key selecting the chrome palette: `light`, `dark`, or `auto`. The value is case-insensitive. When the key is absent or malformed, the viewer SHALL behave as `auto`. The `Theme` setting SHALL NOT affect which colors the plugin's own `[Viewer]` override keys provide — overrides always win over the theme's default for the slot they target.

#### Scenario: Valid Theme in INI
- **WHEN** `multidocviewer.ini` contains `[Viewer] Theme=dark`
- **THEN** the chrome renders with the dark palette on both platforms

#### Scenario: Missing Theme key
- **WHEN** `multidocviewer.ini` is absent or contains no `[Viewer] Theme`
- **THEN** the viewer behaves as `auto`

#### Scenario: Malformed Theme value
- **WHEN** `[Viewer] Theme` contains a non-theme string (e.g. `pink`)
- **THEN** the viewer behaves as `auto`

## MODIFIED Requirements

### Requirement: Background color configuration

The `[Viewer]` section SHALL support a `BackgroundColor` key specifying the page-area background as a hex RGB value (e.g. `#E8E8E8` or `E8E8E8`). The value is case-insensitive and the `#` prefix is optional. The key SHALL override the active theme's page-area background slot; when it is absent or malformed, the page area SHALL use the active theme's default page-area background. The setting SHALL NOT affect the sidebar or the toolbar.

#### Scenario: Valid background color in INI
- **WHEN** `multidocviewer.ini` contains `[Viewer] BackgroundColor=#FFFFFF`
- **THEN** the page area renders with a white background on both platforms, in either theme

#### Scenario: Missing BackgroundColor key
- **WHEN** `multidocviewer.ini` is absent or contains no `[Viewer] BackgroundColor`
- **THEN** the page area renders with the active theme's default page-area background

#### Scenario: Malformed BackgroundColor value
- **WHEN** `[Viewer] BackgroundColor` contains a non-hex string (e.g. `red`)
- **THEN** the page area falls back to the active theme's default page-area background

### Requirement: Sidebar background color configuration

The `[Viewer]` section SHALL support a `SidebarBackground` key specifying the outline sidebar's background as a hex RGB value (e.g. `#F0F0F0` or `F0F0F0`). The value is case-insensitive and the `#` prefix is optional. The key SHALL override the active theme's sidebar-background slot and SHALL NOT affect the page-area background, the toolbar, or the tree's text color; when it is absent or malformed, the sidebar SHALL use the active theme's default sidebar background. The page-area `BackgroundColor` setting SHALL NOT affect the sidebar.

#### Scenario: Valid sidebar background color in INI
- **WHEN** `multidocviewer.ini` contains `[Viewer] SidebarBackground=#F0F0F0`
- **THEN** the outline sidebar renders with the `#F0F0F0` background on both platforms, in either theme, and the page area keeps its own configured background

#### Scenario: Missing SidebarBackground key
- **WHEN** `multidocviewer.ini` is absent or contains no `[Viewer] SidebarBackground`
- **THEN** the sidebar renders with the active theme's default sidebar background

#### Scenario: Malformed SidebarBackground value
- **WHEN** `[Viewer] SidebarBackground` contains a non-hex string (e.g. `sidebar`)
- **THEN** the sidebar falls back to the active theme's default sidebar background

#### Scenario: Overrides target only their slot
- **WHEN** `multidocviewer.ini` sets `[Viewer] SidebarBackground` to a color different from `BackgroundColor`
- **THEN** the page area and the sidebar each render their own configured colors, in either theme, and neither affects the other