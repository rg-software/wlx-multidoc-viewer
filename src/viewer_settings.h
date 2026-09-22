#ifndef VIEWER_SETTINGS_H
#define VIEWER_SETTINGS_H

#include "pluginconfig.h"

#include <algorithm>
#include <cstdint>
#include <string>

// Central UI tuning constants for both viewers (Win32 and Qt).

namespace viewer_settings {

namespace {
// Parses a hex color: "#RRGGBB" (opaque) or "#RRGGBBAA" (with alpha); the '#'
// prefix is optional and the value is case-insensitive. Solid colors keep the
// 0x00RRGGBB form the palette tables use; 8-digit values are repacked to the
// 0xAARRGGBB overlay form. Returns fallback when malformed or empty.
uint32_t parseHexColor(const std::string& value, uint32_t fallback) {
    size_t i = 0;
    if (i < value.size() && value[i] == '#')
        ++i;
    const size_t len = value.size() - i;
    if (len != 6 && len != 8)
        return fallback;
    uint32_t v = 0;
    for (size_t k = 0; k < len; ++k) {
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
        v = (v << 4) | nibble;
    }
    if (len == 6)
        return v; // 0x00RRGGBB
    const uint32_t rgb = (v >> 8) & 0xFFFFFFu;
    const uint32_t a = v & 0xFFu;
    return (a << 24) | rgb; // 0xAARRGGBB
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

// Lowercases ASCII without allocating a locale-aware transform.
std::string toLowerAscii(std::string v) {
    for (char& c : v) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return v;
}
} // anonymous namespace

// Host-signalled dark mode, recorded at load time (Windows: the lcp_darkmode
// bit of the load entry point's ShowFlags; Linux: the Qt color-scheme hint).
// Read once, when the active palette is first resolved.
inline bool& hostDarkFlag() { static bool dark = false; return dark; }
inline void setHostDark(bool dark) { hostDarkFlag() = dark; }
inline bool hostDark() { return hostDarkFlag(); }

enum class Theme { Light, Dark, Auto };

// [Viewer] Theme: light | dark | auto (case-insensitive); anything else is auto.
inline Theme parseTheme(const std::string& value) {
    const std::string v = toLowerAscii(value);
    if (v == "light")
        return Theme::Light;
    if (v == "dark")
        return Theme::Dark;
    return Theme::Auto;
}

// Resolved once from the INI; auto follows the host mode recorded above.
inline Theme activeTheme() {
    const Theme t = parseTheme(PluginConfig::get().get("Viewer").get("Theme"));
    if (t != Theme::Auto)
        return t;
    return hostDark() ? Theme::Dark : Theme::Light;
}

// Named chrome color slots shared by both platforms. Solid slots are 0x00RRGGBB;
// the overlay slots pack alpha in the high byte (0xAARRGGBB).
struct Palette {
    uint32_t pageBg;
    uint32_t sidebarBg;
    uint32_t toolbarBg;
    uint32_t toolbarCheckedTint;
    uint32_t toolbarCheckedRing;
    uint32_t glyph;
    uint32_t treeText;
    uint32_t editBg;
    uint32_t editText;
    uint32_t selectionFill;
    uint32_t searchActiveFill;
    uint32_t searchActivePen;
    uint32_t documentBg;
    uint32_t documentText;
};

// Light table matches the plugin's pre-theme appearance; the overlay slots are
// identical in both tables (selection/search do not change with the theme).
inline const Palette kLightPalette = {
    0xE8E8E8, // pageBg
    0xE8E8E8, // sidebarBg
    0xF0F0F0, // toolbarBg
    0xE4EFFB, // toolbarCheckedTint
    0x9ABEE0, // toolbarCheckedRing
    0x4A4A4A, // glyph
    0x202020, // treeText
    0xFFFFFF, // editBg
    0x000000, // editText
    0x69FFF069, // selectionFill  (a=105, #FFF069)
    0x9600DCDC, // searchActiveFill (a=150, #00DCDC)
    0xFF008282, // searchActivePen  (#008282)
    0xFFFFFF, // documentBg (paper)
    0x000000, // documentText
};

inline const Palette kDarkPalette = {
    0x1E1E1E, // pageBg
    0x2B2B2B, // sidebarBg
    0x333333, // toolbarBg
    0x2E455E, // toolbarCheckedTint
    0x6E94BD, // toolbarCheckedRing
    0xD6D6D6, // glyph
    0xE0E0E0, // treeText
    0x1F1F1F, // editBg
    0xE0E0E0, // editText
    0x69FFF069, // selectionFill
    0x9600DCDC, // searchActiveFill
    0xFF008282, // searchActivePen
    0x262626, // documentBg (a touch lighter than the chrome pageBg)
    0xE0E0E0, // documentText
};

// Resolved once per process at first viewer construction and frozen for the
// process lifetime (Win32 class brushes and icon caches depend on it). The
// selected theme's INI section ([Theme:light] / [Theme:dark]) supplies the
// values; any missing or malformed key falls back to the built-in table above,
// so an absent or partial INI still yields a complete palette.
inline const Palette& activePalette() {
    static const Palette palette = [] {
        const bool dark = (activeTheme() == Theme::Dark);
        Palette p = dark ? kDarkPalette : kLightPalette;
        const auto& section = PluginConfig::get().get(dark ? "Theme:dark" : "Theme:light");
        p.pageBg = parseHexColor(section.get("PageBackground"), p.pageBg);
        p.sidebarBg = parseHexColor(section.get("SidebarBackground"), p.sidebarBg);
        p.toolbarBg = parseHexColor(section.get("ToolbarBackground"), p.toolbarBg);
        p.toolbarCheckedTint =
            parseHexColor(section.get("ToolbarCheckedTint"), p.toolbarCheckedTint);
        p.toolbarCheckedRing =
            parseHexColor(section.get("ToolbarCheckedRing"), p.toolbarCheckedRing);
        p.glyph = parseHexColor(section.get("Glyph"), p.glyph);
        p.treeText = parseHexColor(section.get("TreeText"), p.treeText);
        p.editBg = parseHexColor(section.get("EditBackground"), p.editBg);
        p.editText = parseHexColor(section.get("EditText"), p.editText);
        p.selectionFill = parseHexColor(section.get("SelectionFill"), p.selectionFill);
        p.searchActiveFill = parseHexColor(section.get("SearchActiveFill"), p.searchActiveFill);
        p.searchActivePen = parseHexColor(section.get("SearchActivePen"), p.searchActivePen);
        p.documentBg = parseHexColor(section.get("DocumentBackground"), p.documentBg);
        p.documentText = parseHexColor(section.get("DocumentText"), p.documentText);
        return p;
    }();
    return palette;
}

inline constexpr int kWheelStepPx = 60;         // px per wheel notch (continuous mode)
inline constexpr int kKeyboardStepPx = 60;      // px per Up/Down arrow key press
inline constexpr int kScrollBarLineStepPx = 20; // px per scrollbar line click
inline constexpr int kPageGap = 4;              // gap between pages in continuous strip
inline constexpr int kPageMargin = 8;           // margin around page area
inline constexpr int kBufferPages = 3;          // extra pages rendered above/below viewport
inline constexpr int kCacheWindowPages = 20;    // max pages kept in the render cache (LRU)
inline constexpr double kSelectionHitTolerancePx = 3.0; // px radius for text hit-testing
inline constexpr double kLinkHitTolerancePx = 4.0;      // px radius for hyperlink hit-testing

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

// Reflowable-document font size (em, in points) read from [Viewer] FontSize.
// This is the text layout size, not a raster zoom: it sets characters per line
// (page width / em), so changing it re-flows the document and changes the page
// count. Clamped to a readable range; the default (11) matches muPDF's built-in
// layout, so an absent key changes nothing. Requires a host restart.
inline constexpr int kReflowFontSizeMin = 8;
inline constexpr int kReflowFontSizeMax = 32;
inline int kReflowFontSize = [] {
    const int parsed = parseInt(PluginConfig::get().get("Viewer").get("FontSize"), 11);
    return (std::max)(kReflowFontSizeMin, (std::min)(kReflowFontSizeMax, parsed));
}();

} // namespace viewer_settings

#endif // VIEWER_SETTINGS_H
