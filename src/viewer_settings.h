#ifndef VIEWER_SETTINGS_H
#define VIEWER_SETTINGS_H

// Central UI tuning constants for both viewers (Win32 and Qt).
// Edit here and rebuild; there is no runtime config by design.

namespace viewer_settings {

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
inline constexpr int kSidebarBaseWidth = 180;  // outline sidebar width (logical px)
inline constexpr int kIconBaseSize = 24;        // toolbar icon size (logical px)

} // namespace viewer_settings

#endif // VIEWER_SETTINGS_H
