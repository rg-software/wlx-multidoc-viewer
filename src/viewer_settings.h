#ifndef VIEWER_SETTINGS_H
#define VIEWER_SETTINGS_H

// Central UI tuning constants for both viewers (Win32 and Qt).
// Edit here and rebuild; there is no runtime config by design.

namespace viewer_settings {

inline constexpr int kWheelStepPx = 60;         // px per wheel notch (continuous mode)
inline constexpr int kKeyboardStepPx = 60;      // px per Up/Down arrow key press
inline constexpr int kScrollBarLineStepPx = 20; // px per scrollbar line click
inline constexpr int kInfoPanelHeight = 22;     // info panel / info bar height
inline constexpr int kPageGap = 4;              // gap between pages in continuous strip
inline constexpr int kPageMargin = 8;           // margin around page area
inline constexpr int kBufferPages = 3;          // extra pages rendered above/below viewport
inline constexpr long long kMaxStripHeight = 1500000; // continuous-strip height cap

} // namespace viewer_settings

#endif // VIEWER_SETTINGS_H
