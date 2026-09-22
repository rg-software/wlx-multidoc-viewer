#include "wlxplugin.h"

#include <QDebug>

#ifdef _WIN32
#include "viewer_win32.h"
#else
#include "viewer.h"
#include <QApplication>
#include <QPalette>
#include <QStyleHints>
#include <QWidget>

static bool ensureQApplication() {
    if (QCoreApplication::instance())
        return true;
    static int argc = 1;
    static char arg0[] = "wlx-multidoc-viewer";
    static char* argv[] = { arg0, nullptr };
    new QApplication(argc, argv);
    return true;
}
#endif

#include "viewer_settings.h"

// Records the host's ambient mode once, before any viewer constructs and thus
// before the active palette is first resolved (it is frozen thereafter).
// Windows: the lcp_darkmode bit TC/DC passes in ShowFlags. Linux: the Qt
// color-scheme hint (6.5+; older Qt falls back to the window palette lightness).
static void recordHostTheme(int showFlags) {
#if defined(_WIN32)
    viewer_settings::setHostDark((showFlags & lcp_darkmode) != 0);
#else
    Q_UNUSED(showFlags)
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    viewer_settings::setHostDark(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
    viewer_settings::setHostDark(QGuiApplication::palette().color(QPalette::Window).lightness() < 128);
#endif
#endif
}

#define SUPPORTED_EXTENSIONS \
    "EXT=\"PDF\"|EXT=\"XPS\"|EXT=\"OXPS\"|" \
    "EXT=\"EPUB\"|EXT=\"MOBI\"|EXT=\"FB2\"|" \
    "EXT=\"CBZ\"|EXT=\"CBR\"|EXT=\"CB7\"|EXT=\"ZIP\"|" \
    "EXT=\"JPEG\"|EXT=\"JPG\"|EXT=\"PNG\"|EXT=\"TIFF\"|" \
    "EXT=\"GIF\"|EXT=\"BMP\"|EXT=\"WEBP\"|" \
    "EXT=\"DJVU\"|EXT=\"DJV\"|EXT=\"CHM\""

static_assert(sizeof(SUPPORTED_EXTENSIONS) <= 260,
    "Detect string exceeds WLX buffer limit of 260 chars");

// The host (Total Commander / Double Commander) owns Ctrl+F in the Lister and
// routes it to ListSearchDialog instead of delivering a key to the plugin
// window. Focusing the built-in toolbar search box is our "search dialog".
static bool focusFindForListWin(HANDLE ListWin) {
#if defined(_WIN32)
    HWND hViewer = static_cast<HWND>(ListWin);
    auto* viewer = hViewer
        ? reinterpret_cast<ViewerWin32*>(GetWindowLongPtrW(hViewer, GWLP_USERDATA))
        : nullptr;
#else
    auto* viewer = static_cast<ViewerWidget*>(ListWin);
#endif
    if (!viewer)
        return false;
    viewer->focusFind();
    return true;
}

// Shared document-open core used by both the ANSI and wide (W) entry points.
// The WLX interface hands plugins a narrow char* file name in the ANSI code
// page, which cannot represent CJK/Cyrillic filenames on a mismatched system
// locale. Total Commander (and Double Commander on Windows) call the `...W`
// variants with UTF-16 whenever they are exported, so every viewer path must go
// through here with a proper QString.
static HANDLE loadDocumentIntoViewer(HANDLE ParentWin, const QString& path, int showFlags) {
    if (!ParentWin)
        return nullptr;

#if defined(_WIN32)
    recordHostTheme(showFlags);
    auto* viewer = new ViewerWin32(static_cast<HWND>(ParentWin));
    if (!viewer->loadDocument(path)) {
        qWarning() << "ListLoad: failed to load" << path;
        delete viewer;
        return nullptr;
    }
    return static_cast<HANDLE>(viewer->hwnd());
#else
    ensureQApplication();
    recordHostTheme(showFlags);

    auto* parent = static_cast<QWidget*>(ParentWin);
    if (!parent)
        return nullptr;

    auto* viewer = new ViewerWidget(parent);
    viewer->show();

    if (!viewer->loadDocument(path)) {
        delete viewer;
        return nullptr;
    }

    return static_cast<HANDLE>(viewer);
#endif
}

DCPCALL HANDLE ListLoad(HANDLE ParentWin, char* FileToLoad, int ShowFlags) {
    const QString path = QString::fromLocal8Bit(FileToLoad);
    qDebug() << "ListLoad:" << path;
    return loadDocumentIntoViewer(ParentWin, path, ShowFlags);
}

// Wide (UTF-16 filename) entry points — preferred by TC/DC when exported.
#ifdef _WIN32
DCPCALL HANDLE ListLoadW(HANDLE ParentWin, wchar_t* FileToLoad, int ShowFlags) {
    const QString path = QString::fromWCharArray(FileToLoad);
    qDebug() << "ListLoadW:" << path;
    return loadDocumentIntoViewer(ParentWin, path, ShowFlags);
}
#endif

DCPCALL int ListLoadNext(HANDLE ParentWin, HANDLE PluginWin,
                          char* FileToLoad, int ShowFlags) {
    Q_UNUSED(ParentWin)
    recordHostTheme(ShowFlags);

#if defined(_WIN32)
    HWND hViewer = static_cast<HWND>(PluginWin);
    if (!hViewer)
        return LISTPLUGIN_ERROR;

    auto* viewer = reinterpret_cast<ViewerWin32*>(
        GetWindowLongPtrW(hViewer, GWLP_USERDATA));
    if (!viewer)
        return LISTPLUGIN_ERROR;

    if (viewer->loadDocument(QString::fromLocal8Bit(FileToLoad)))
        return LISTPLUGIN_OK;
#elif defined(__linux__)
    auto* viewer = static_cast<ViewerWidget*>(PluginWin);
    if (!viewer)
        return LISTPLUGIN_ERROR;

    if (viewer->loadDocument(QString::fromLocal8Bit(FileToLoad)))
        return LISTPLUGIN_OK;
#endif

    return LISTPLUGIN_ERROR;
}

#ifdef _WIN32
DCPCALL int ListLoadNextW(HANDLE ParentWin, HANDLE PluginWin,
                          wchar_t* FileToLoad, int ShowFlags) {
    Q_UNUSED(ParentWin)
    recordHostTheme(ShowFlags);

    HWND hViewer = static_cast<HWND>(PluginWin);
    if (!hViewer)
        return LISTPLUGIN_ERROR;
    auto* viewer = reinterpret_cast<ViewerWin32*>(
        GetWindowLongPtrW(hViewer, GWLP_USERDATA));
    if (!viewer)
        return LISTPLUGIN_ERROR;

    if (viewer->loadDocument(QString::fromWCharArray(FileToLoad)))
        return LISTPLUGIN_OK;

    return LISTPLUGIN_ERROR;
}

DCPCALL void ListCloseWindowW(HANDLE ListWin); // defined below after ListCloseWindow

DCPCALL int ListSearchDialogW(HWND ListWin, int FindNext, wchar_t* FindText) {
    Q_UNUSED(FindNext)
    Q_UNUSED(FindText)
    return focusFindForListWin(ListWin) ? LISTPLUGIN_OK : LISTPLUGIN_ERROR;
}
#endif // _WIN32

DCPCALL void ListCloseWindow(HANDLE ListWin) {
    if (!ListWin)
        return;

#ifdef _WIN32
    HWND hViewer = static_cast<HWND>(ListWin);
    auto* viewer = reinterpret_cast<ViewerWin32*>(
        GetWindowLongPtrW(hViewer, GWLP_USERDATA));
    if (viewer) {
        viewer->closeDocument();
        delete viewer;
    }
#else
    auto* viewer = static_cast<ViewerWidget*>(ListWin);
    viewer->closeDocument();
    viewer->hide();
    viewer->deleteLater();
#endif
}

#ifdef _WIN32
DCPCALL void ListCloseWindowW(HANDLE ListWin) {
    ListCloseWindow(ListWin);
}
#endif // _WIN32

DCPCALL void ListGetDetectString(char* DetectString, int maxlen) {
    snprintf(DetectString, maxlen - 1, "%s", SUPPORTED_EXTENSIONS);
}

DCPCALL int ListSearchDialog(HWND ListWin, int FindNext) {
    Q_UNUSED(FindNext)
    return focusFindForListWin(ListWin) ? LISTPLUGIN_OK : LISTPLUGIN_ERROR;
}

DCPCALL int ListSendCommand(HWND ListWin, int Command, int Parameter) {
#ifdef _WIN32
    HWND hViewer = static_cast<HWND>(ListWin);
    auto* viewer = hViewer ? reinterpret_cast<ViewerWin32*>(GetWindowLongPtrW(hViewer, GWLP_USERDATA))
                           : nullptr;
    Q_UNUSED(Parameter)

    if (viewer && Command == lc_copy) {
        QString text;
        if (auto* c = viewer->controller(); c && c->hasSelection()) {
            // Copy the actual selected text (not the page indicator).
            text = c->selectedText();
        }
        if (!text.isEmpty()) {
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                QByteArray utf16(reinterpret_cast<const char*>(text.utf16()),
                                 (text.size() + 1) * static_cast<int>(sizeof(ushort)));
                HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, utf16.size());
                if (h) {
                    void* p = GlobalLock(h);
                    memcpy(p, utf16.constData(), utf16.size());
                    GlobalUnlock(h);
                    SetClipboardData(CF_UNICODETEXT, h);
                }
                CloseClipboard();
            }
        }
        return LISTPLUGIN_OK;
    }
#else
    Q_UNUSED(ListWin)
    Q_UNUSED(Command)
    Q_UNUSED(Parameter)
#endif
    return LISTPLUGIN_OK;
}

DCPCALL void ListSetDefaultParams(ListDefaultParamStruct* dps) {
    Q_UNUSED(dps)
}

#ifdef _WIN32
DCPCALL int ListSendCommandW(HWND ListWin, int Command, int Parameter) {
    return ListSendCommand(ListWin, Command, Parameter);
}
#endif // _WIN32
