## Context

The Win32 viewer (`viewer_win32.cpp`) handles `WM_MOUSEWHEEL` but has no `WM_LBUTTONDOWN`/`WM_MOUSEMOVE`/`WM_LBUTTONUP` handling. The Qt viewer (`viewer.cpp`) similarly has no mouse press/move/release events. UI tuning constants (scroll step 60, scrollbar step 20, info panel height 22, page gap 4, margins, buffer pages 3) are hardcoded across three files with some values duplicated.

## Goals / Non-Goals

**Goals:**
- Left-click drag pans the viewport in continuous mode (both axes)
- Cursor shows grabbing hand during drag, restores on release
- All tuning constants in one `viewer_settings.h` header
- Both Win32 and Qt viewers support drag panning

**Non-Goals:**
- Touch/stylus drag support (future work)
- Kinetic/inertial scrolling
- Drag in paged mode (spec says inactive)

## Decisions

### 1. Drag state lives on the viewer, not the controller

The controller owns document logic (page nav, zoom, render). Drag is a viewport-level interaction that only changes `m_scrollX`/`m_scrollY` — same as wheel/keyboard. Keeping drag state (tracking flag, last mouse position) on the viewer avoids bloating the controller with UI state.

**Alternative considered**: Controller-level drag with `startDrag`/`updateDrag`/`endDrag` methods. Rejected — the controller already has no concept of pixel-level scroll position; it delegates that to the viewer.

### 2. Direct 1:1 pixel delta (no acceleration)

Mouse delta maps 1:1 to scroll pixels. SumatraPDF uses the same approach. Acceleration adds complexity with no clear benefit for document viewing.

### 3. Settings as an inline header (`viewer_settings.h`)

A simple `constexpr` header. No runtime config file, no INI parsing — TC/WLX plugins have no standard config mechanism. Users who want to change values edit the header and rebuild.

**Alternative considered**: Runtime config via `GetPrivateProfileInt` (Windows INI). Rejected — adds complexity for an edge case; the values are rarely tuned.

### 4. Cursor via Win32 `SetCursor` / Qt `setCursor`

Win32: handle `WM_SETCURSOR` to override cursor when over client area during drag. Use `LoadCursor(NULL, IDC_HAND)` for grab, `IDC_ARROW` for default.

Qt: `setCursor(Qt::PointingHandCursor)` on press, `unsetCursor()` on release.

## Platform-specific code

| Component | Win32 | Qt |
|---|---|---|
| Drag start | `WM_LBUTTONDOWN` → set tracking flag, `SetCapture` | `mousePressEvent` → set flag |
| Drag move | `WM_MOUSEMOVE` → compute delta, update scroll | `mouseMoveEvent` → same logic |
| Drag end | `WM_LBUTTONUP` → clear flag, `ReleaseCapture` | `mouseReleaseEvent` → clear flag |
| Cursor | `WM_SETCURSOR` + `SetCursor(LoadCursor(...))` | `setCursor()` / `unsetCursor()` |
| Guard | Only in continuous mode (check `!isPagedMode()`) | Same |

## Risks / Trade-offs

- **TC may intercept mouse clicks** → If `WM_LBUTTONDOWN` is consumed by TC before reaching our window, drag won't work. Mitigation: `SetCapture` ensures we receive subsequent `MOUSEMOVE`/`BUTTONUP` even if cursor leaves the window. Test with TC; if clicks are eaten, consider using `WM_MOUSEACTIVATE` to return `MA_ACTIVATE`.
- **Strip re-render on drag end** → Drag only scrolls (no re-render needed unless past buffer). `needsStripRerender()` already handles this — same as wheel.
- **Header-only settings** → Editing requires recompilation. Acceptable for a plugin with no user-facing config dialog.
