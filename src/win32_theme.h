#ifndef WIN32_THEME_H
#define WIN32_THEME_H

#include "viewer_settings.h"

#ifdef _WIN32

#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

namespace win32_theme {

// Applies the active chrome theme to a Win32 window so its frame and standard
// scrollbars follow the resolved palette (issue #14). The active theme is
// authoritative, not the OS setting: dark theme forces the dark chrome, light
// reverts it. Two cooperating mechanisms:
//   * DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE, TRUE/FALSE)
//     toggles the window's frame/scrollbar dark state; attribute 20 is the
//     current value, 19 the pre-20H1 name, so each is tried on older builds.
//   * SetWindowTheme(L"DarkMode_Explorer") gives the standard WS_* and common
//     control scrollbars an always-dark Explorer variant (reset with nulls in
//     light mode).
inline void applyChromeTheme(HWND hwnd) {
    const bool dark =
        (viewer_settings::activeTheme() == viewer_settings::Theme::Dark);
    const BOOL state = dark ? TRUE : FALSE;
    if (DwmSetWindowAttribute(hwnd, 20, &state, sizeof(state)) != S_OK)
        DwmSetWindowAttribute(hwnd, 19, &state, sizeof(state));
    SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : nullptr, nullptr);
}

} // namespace win32_theme

#endif // _WIN32
#endif // WIN32_THEME_H