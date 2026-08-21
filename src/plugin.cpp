#include "wlxplugin.h"

#include <QDebug>

#ifdef _WIN32
#include "viewer_win32.h"
#else
#include "viewer.h"
#include <QApplication>
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

#define SUPPORTED_EXTENSIONS \
    "EXT=\"PDF\"|EXT=\"XPS\"|EXT=\"OXPS\"|" \
    "EXT=\"EPUB\"|EXT=\"MOBI\"|EXT=\"FB2\"|" \
    "EXT=\"CBZ\"|EXT=\"CBR\"|EXT=\"CB7\"|" \
    "EXT=\"HTML\"|EXT=\"HTM\"|EXT=\"MD\"|EXT=\"TXT\"|" \
    "EXT=\"JPEG\"|EXT=\"JPG\"|EXT=\"PNG\"|EXT=\"TIFF\"|" \
    "EXT=\"GIF\"|EXT=\"BMP\"|EXT=\"WEBP\"|" \
    "EXT=\"DJVU\"|EXT=\"DJV\""

static_assert(sizeof(SUPPORTED_EXTENSIONS) <= 260,
    "Detect string exceeds WLX buffer limit of 260 chars");

DCPCALL HANDLE ListLoad(HANDLE ParentWin, char* FileToLoad, int ShowFlags) {
    Q_UNUSED(ShowFlags)

    if (!ParentWin)
        return nullptr;

    QString path = QString::fromLocal8Bit(FileToLoad);
    qDebug() << "ListLoad:" << path;

#ifdef _WIN32
    auto* viewer = new ViewerWin32(static_cast<HWND>(ParentWin));
    if (!viewer->loadDocument(path)) {
        qWarning() << "ListLoad: failed to load" << path;
        delete viewer;
        return nullptr;
    }
    return static_cast<HANDLE>(viewer->hwnd());
#else
    ensureQApplication();

    auto* viewer = new ViewerWidget(nullptr);

    viewer->show();

    HWND hViewer = reinterpret_cast<HWND>(viewer->winId());
    HWND hParent = static_cast<HWND>(ParentWin);
    SetParent(hViewer, hParent);

    RECT rc;
    GetClientRect(hParent, &rc);
    SetWindowPos(hViewer, nullptr, 0, 0, rc.right, rc.bottom,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    viewer->resize(rc.right, rc.bottom);

    if (!viewer->loadDocument(QString::fromLocal8Bit(FileToLoad))) {
        delete viewer;
        return nullptr;
    }

    return static_cast<HANDLE>(viewer);
#endif
}

DCPCALL int ListLoadNext(HANDLE ParentWin, HANDLE PluginWin,
                          char* FileToLoad, int ShowFlags) {
    Q_UNUSED(ParentWin)
    Q_UNUSED(ShowFlags)

#ifdef _WIN32
    HWND hViewer = static_cast<HWND>(PluginWin);
    if (!hViewer)
        return LISTPLUGIN_ERROR;

    auto* viewer = reinterpret_cast<ViewerWin32*>(
        GetWindowLongPtrW(hViewer, GWLP_USERDATA));
    if (!viewer)
        return LISTPLUGIN_ERROR;

    if (viewer->loadDocument(QString::fromLocal8Bit(FileToLoad)))
        return LISTPLUGIN_OK;
#else
    auto* viewer = static_cast<ViewerWidget*>(PluginWin);
    if (!viewer)
        return LISTPLUGIN_ERROR;

    if (viewer->loadDocument(QString::fromLocal8Bit(FileToLoad)))
        return LISTPLUGIN_OK;
#endif

    return LISTPLUGIN_ERROR;
}

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

DCPCALL void ListGetDetectString(char* DetectString, int maxlen) {
    snprintf(DetectString, maxlen - 1, "%s", SUPPORTED_EXTENSIONS);
}

DCPCALL int ListSearchDialog(HWND ListWin, int FindNext) {
    Q_UNUSED(ListWin)
    Q_UNUSED(FindNext)
    return LISTPLUGIN_OK;
}

DCPCALL int ListSendCommand(HWND ListWin, int Command, int Parameter) {
#ifdef _WIN32
    HWND hViewer = static_cast<HWND>(ListWin);
    auto* viewer = hViewer ? reinterpret_cast<ViewerWin32*>(GetWindowLongPtrW(hViewer, GWLP_USERDATA))
                           : nullptr;
    Q_UNUSED(Parameter)

    if (viewer && Command == lc_copy) {
        QString text;
        if (auto* c = viewer->controller(); c && c->hasDocument())
            text = QString("%1 / %2").arg(c->currentPage()).arg(c->pageCount());
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
