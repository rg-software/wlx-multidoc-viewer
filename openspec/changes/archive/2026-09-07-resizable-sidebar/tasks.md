## 1. Shared constants and backend interface

- [x] 1.1 In `viewer_settings.h`, add the INI-fed defaults alongside `kSidebarBaseWidth = 180`: `kSidebarInitialWidth` (`uint32_t`-free `int`, parsed from `[Viewer] SidebarWidth`, clamped to ≥ `kSidebarMinWidth`, default 180) and `kSidebarVisibleByDefault` (parsed from `[Viewer] SidebarVisible`, default false), both read via the `PluginConfig::get()`/`[Viewer]` pattern used by `kBackgroundColor`; add `kSidebarMinWidth = 80` and `kSidebarGripWidthPx = 6`; update the `kSidebarBaseWidth` comment to note it is the fallback default and user-resizable per session.
- [x] 1.2 Add `setWidthChangedHandler(std::function<void(int logicalPx)>)` to the shared `SidebarBackend` class in `sidebar.h`.

## 2. Win32 sidebar grip, resize, and left-scroll reset

- [x] 2.1 Replace the hard-coded `widthPx()` with a member `m_baseWidthPx` initialized from `viewer_settings::kSidebarInitialWidth`; add `setBaseWidth(int logicalPx)` so the viewer can push drag results.
- [x] 2.2 Create a 6px-wide grip child `HWND` on the right edge of `SidebarWin32`'s panel; adjust the `WM_SIZE` handler so the tree fills `panel − grip`.
- [x] 2.3 Implement drag tracking in the panel `wndProc`: `WM_SETCURSOR` (IDC_SIZEWE), `WM_LBUTTONDOWN` (SetCapture, capture origin), `WM_MOUSEMOVE` (candidate logical px from device coords via `m_dpiScale`, invoke shared handler), `WM_LBUTTONUP` / `WM_CAPTURECHANGED` (ReleaseCapture, done).
- [x] 2.4 Reset the tree's horizontal scroll to the left edge: `SendMessage(m_tree, WM_HSCROLL, SB_LEFT, 0)` immediately after `TreeView_EnsureVisible` in `selectEntry`, and at the end of `clearEntries`, both inside the existing `WM_SETREDRAW` guard with a single `InvalidateRect` afterward.
- [x] 2.5 In `ViewerWin32`, wire the sidebar's new handler to clamp the candidate width to `[kSidebarMinWidth, max(kSidebarMinWidth, clientLogicalW / 3)]` (converted to device px) and re-run the chrome chain: `layoutChrome` → `setLeftChrome(sidebarLeft())` → `relayout(m_scrollY)` → `onControllerChanged()`.
- [x] 2.6 Apply the INI default visibility on document load: replace the hard-coded hidden start (`showHideSidebar(false)` / `m_sidebar->setVisible(false)` at open) with `showHideSidebar(viewer_settings::kSidebarVisibleByDefault)` when the document has an outline.
- [x] 2.7 Build the Win32 preset (`cmake --preset windows-x64-release && cmake --build --preset windows-release`); confirm the sidebar starts at `kSidebarInitialWidth` (180 logical px by default), is shown on load when `[Viewer] SidebarVisible=true`, drag-resizes between min and max bounds, reload retains width, and a new viewer window resets to the configured initial width.

## 3. Qt sidebar grip, resize, and left-scroll reset

- [x] 3.1 Add a 6px grip `QWidget` as the last child of `m_midLayout` in `SidebarQt`, with `SizeHorCursor`; ensure the tree is inserted without stretch (stretch = 1) so the layout splits correctly; initialize `setFixedWidth(viewer_settings::kSidebarInitialWidth)`.
- [x] 3.2 Implement `mousePressEvent` / `mouseMoveEvent` / `mouseReleaseEvent` in the grip: track drag with a `m_resizing` flag, compute candidate logical width, invoke the shared handler, and call `m_tree->setCursor(...)` as the cursor enters/leaves the grip region.
- [x] 3.3 Reset `m_tree->horizontalScrollBar()->setValue(0)` at the end of `selectEntry` (after `scrollToItem`) and at the end of `clearEntries`, with the `m_resizing` reentrancy flag preventing a feedback loop between `setWidth` and mouse-move events.
- [x] 3.4 In `ViewerWidget`, wire the sidebar's new handler to clamp the candidate logical width to `[kSidebarMinWidth, max(kSidebarMinWidth, clientLogicalW / 3)]`, call `m_sidebar->setWidth(logicalPx)`, update `m_controller->setLeftChrome(logicalPx)`, and re-run `relayout(m_scrollY)` + `onControllerChanged()`.
- [x] 3.5 Apply the INI default visibility on document load: replace the hard-coded hidden start in `openDocument` (`m_sidebarVisible = false; m_sidebar->setVisible(false)`) with `m_sidebarVisible = viewer_settings::kSidebarVisibleByDefault` when the document has an outline.
- [ ] 3.6 Build the Qt preset (`cmake --preset linux-release && cmake --build --preset linux-release`); confirm the same scenarios as the Win32 build plus live layout reflow during the drag, the INI-driven initial width and visibility, and that the grip does not steal focus or clicks from the tree.

## 4. Cross-platform verification

- [ ] 4.1 Verify both presets compile cleanly on Windows and Linux with no new warnings.
- [ ] 4.2 Run an interactive smoke test on both platforms: sidebar starts at the INI initial width (180 logical px without `[Viewer] SidebarWidth`); with `[Viewer] SidebarVisible=true` a document with an outline opens with the sidebar shown; drag to min (clamped at 80) and to max (half the page area); DPI change preserves the logical width; hiding then re-showing the sidebar retains the resized width; loading a new document retains the width within the same viewer window; creating a new viewer window resets to the configured initial width; after navigating to a TOC entry, the tree's horizontal scroll is at the left edge; manual horizontal scroll is preserved between reveals.