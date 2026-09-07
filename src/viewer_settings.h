#ifndef VIEWER_SETTINGS_H
#define VIEWER_SETTINGS_H

#include "pluginconfig.h"

#include <algorithm>
#include <cstdint>
#include <string>

// Central UI tuning constants for both viewers (Win32 and Qt).

namespace viewer_settings {

namespace {
// Parses a hex RGB like "#E8E8E8" or "E8E8E8" (optional '#', 6 hex digits).
// Returns fallback when the string is malformed or empty.
uint32_t parseHexColor(const std::string& value, uint32_t fallback) {
    size_t i = 0;
    if (i < value.size() && value[i] == '#')
        ++i;
    const size_t len = value.size() - i;
    if (len != 6)
        return fallback;
    uint32_t rgb = 0;
    for (size_t k = 0; k < 6; ++k) {
        const char c = value[i + k];
        uint32_t nibble;
        if (c >= '0' && c <= '9')
            nibble = static_cast<uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f')
            nibble = static_cast<uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            nibble = static_cast<uint32_t>(c - 'A' + 10);
        else
            return fallback;
        rgb = (rgb << 4) | nibble;
    }
    return rgb;
}

// Parses a non-negative decimal integer; returns fallback when malformed.
int parseInt(const std::string& value, int fallback) {
    if (value.empty())
        return fallback;
    int out = 0;
    for (char c : value) {
        if (c < '0' || c > '9')
            return fallback;
        out = out * 10 + (c - '0');
    }
    return out;
}

// Parses a boolean: true when the trimmed string is "1"/"true"/"yes"/"on".
bool parseBool(const std::string& value, bool fallback) {
    if (value.empty())
        return fallback;
    std::string v = value;
    for (char& c : v) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    if (v == "1" || v == "true" || v == "yes" || v == "on")
        return true;
    if (v == "0" || v == "false" || v == "no" || v == "off")
        return false;
    return fallback;
}
} // anonymous namespace

// Page-area background color (0x00RRGGBB), read from the plugin INI's
// [Viewer] BackgroundColor key at startup; falls back to 0xE8E8E8.
inline uint32_t kBackgroundColor = parseHexColor(
    PluginConfig::get().get("Viewer").get("BackgroundColor"), 0xE8E8E8);

inline constexpr int kWheelStepPx = 60;         // px per wheel notch (continuous mode)
inline constexpr int kKeyboardStepPx = 60;      // px per Up/Down arrow key press
inline constexpr int kScrollBarLineStepPx = 20; // px per scrollbar line click
inline constexpr int kPageGap = 4;              // gap between pages in continuous strip
inline constexpr int kPageMargin = 8;           // margin around page area
inline constexpr int kBufferPages = 3;          // extra pages rendered above/below viewport
inline constexpr int kCacheWindowPages = 20;    // max pages kept in the render cache (LRU)
inline constexpr double kSelectionHitTolerancePx = 3.0; // px radius for text hit-testing

// Toolbar chrome: logical (DPI-independent) base sizes, scaled by dpiScale to
// device pixels by the viewers.
inline constexpr int kToolbarBaseHeight = 40;   // toolbar strip height (logical px)
inline constexpr int kSidebarBaseWidth = 180;   // outline sidebar fallback default (logical px);
                                                // user-resizable per session, see SidebarWidth
inline constexpr int kIconBaseSize = 24;        // toolbar icon size (logical px)

// Sidebar bounds and resize grip.
inline constexpr int kSidebarMinWidth = 80;     // narrowest the sidebar can be dragged (logical px)
inline constexpr int kSidebarGripWidthPx = 6;   // right-edge drag handle width (device px)

// INI-fed defaults, read from the plugin's [Viewer] section at startup (never
// written back). kSidebarInitialWidth feeds a fresh viewer window's starting
// width; kSidebarVisibleByDefault decides whether an outlined document starts
// with the sidebar shown. The user-resized width is session-only and never
// persisted.
inline int kSidebarInitialWidth = [] {
    const int parsed = parseInt(PluginConfig::get().get("Viewer").get("SidebarWidth"), kSidebarBaseWidth);
    return (std::max)(kSidebarMinWidth, parsed);
}();
inline bool kSidebarVisibleByDefault = parseBool(
    PluginConfig::get().get("Viewer").get("SidebarVisible"), false);

} // namespace viewer_settings

#endif // VIEWER_SETTINGS_H
