#ifndef UI_STRINGS_H
#define UI_STRINGS_H

#include <QString>

// Single source of truth for user-facing (UI) strings, shared by the Qt and
// Win32 backends so tooltips/messages never drift between platforms. The Win32
// backend converts these to wide strings via QString::utf16() where the native
// API needs wchar_t. Keep everything here plain and side-effect-free.
namespace ui_strings {

// --- Toolbar tooltips -----------------------------------------------------
inline QString tooltipToggleSidebar()    { return QStringLiteral("Toggle outline sidebar"); }
inline QString tooltipPrint()            { return QStringLiteral("Print"); }
inline QString tooltipPrevPage()         { return QStringLiteral("Previous page"); }
inline QString tooltipNextPage()         { return QStringLiteral("Next page"); }
inline QString tooltipToggleMode()       { return QStringLiteral("Toggle paged / continuous"); }
inline QString tooltipPresentation()     { return QStringLiteral("Single / double / double with cover"); }
inline QString tooltipFitMode()          { return QStringLiteral("Fit mode (manual / page / width)"); }
inline QString tooltipRotateLeft()       { return QStringLiteral("Rotate left"); }
inline QString tooltipRotateRight()      { return QStringLiteral("Rotate right"); }
inline QString tooltipZoomOut()          { return QStringLiteral("Zoom out"); }
inline QString tooltipZoomIn()           { return QStringLiteral("Zoom in"); }
inline QString tooltipFindPrev()         { return QStringLiteral("Previous match"); }
inline QString tooltipFindNext()         { return QStringLiteral("Next match"); }
inline QString tooltipMatchCase()        { return QStringLiteral("Match case"); }
inline QString tooltipCopy()             { return QStringLiteral("Copy selection"); }

// --- Toolbar status / edit-text --------------------------------------------
inline QString pageCountSuffix()         { return QStringLiteral("/ %1"); }
inline QString matchCountFraction()      { return QStringLiteral("%1 / %2"); }
inline QString findSearching()           { return QStringLiteral("Searching"); }
inline QString findNoMatch()             { return QStringLiteral("No matches"); }

// --- Print (Qt message box) ---------------------------------------------------
inline QString printTitle()              { return QStringLiteral("Print"); }
inline QString printFailedMessage()      { return QStringLiteral("Printing failed: the printer rejected the document."); }

// --- Go-to-page dialog (Qt only) ----------------------------------------------
inline QString gotoPageTitle()           { return QStringLiteral("Go to page"); }
inline QString gotoPagePrompt()          { return QStringLiteral("Page number:"); }

} // namespace ui_strings

#endif // UI_STRINGS_H