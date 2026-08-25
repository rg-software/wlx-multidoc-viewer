#ifndef PRINT_WIN32_H
#define PRINT_WIN32_H

class ViewerController;

#ifdef Q_OS_WIN
struct HWND__;
typedef HWND__* HWND;
#endif

// Opens the native print dialog for the document and spools the chosen range at
// printer resolution. See print_win32.cpp. Returns without side effects when
// the user cancels.
bool printDocumentWin32(HWND hParent, ViewerController* controller);

#endif // PRINT_WIN32_H