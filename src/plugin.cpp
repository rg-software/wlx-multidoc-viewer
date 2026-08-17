#include "wlxplugin.h"
#include "viewer.h"

#include <QApplication>
#include <QWidget>
#include <QFrame>

static_assert(sizeof(HWND) >= sizeof(void*), "HWND must hold a pointer");

DCPCALL HANDLE ListLoad(HANDLE ParentWin, char* FileToLoad, int ShowFlags) {
    Q_UNUSED(ShowFlags)

    auto* parent = static_cast<QWidget*>(ParentWin);
    if (!parent)
        return nullptr;

    auto* viewer = new ViewerWidget(parent);
    if (!viewer->loadDocument(QString::fromLocal8Bit(FileToLoad))) {
        delete viewer;
        return nullptr;
    }

    viewer->show();
    return static_cast<HANDLE>(viewer);
}

DCPCALL int ListLoadNext(HANDLE ParentWin, HANDLE PluginWin,
                          char* FileToLoad, int ShowFlags) {
    Q_UNUSED(ParentWin)
    Q_UNUSED(ShowFlags)

    auto* viewer = static_cast<ViewerWidget*>(PluginWin);
    if (!viewer)
        return LISTPLUGIN_ERROR;

    if (viewer->loadDocument(QString::fromLocal8Bit(FileToLoad)))
        return LISTPLUGIN_OK;

    return LISTPLUGIN_ERROR;
}

DCPCALL void ListCloseWindow(HANDLE ListWin) {
    auto* viewer = static_cast<ViewerWidget*>(ListWin);
    if (!viewer)
        return;

    viewer->closeDocument();
    delete viewer;
}

#define SUPPORTED_EXTENSIONS \
    "EXT=\"PDF\";EXT=\"XPS\";EXT=\"OXPS\";" \
    "EXT=\"EPUB\";EXT=\"MOBI\";EXT=\"FB2\";EXT=\"FB2Z\";" \
    "EXT=\"CBZ\";EXT=\"CBR\";EXT=\"CB7\";EXT=\"CBT\";" \
    "EXT=\"HTML\";EXT=\"HTM\";EXT=\"MD\";EXT=\"MARKDOWN\";EXT=\"TXT\";" \
    "EXT=\"JPEG\";EXT=\"JPG\";EXT=\"PNG\";EXT=\"TIFF\";EXT=\"TIF\";" \
    "EXT=\"GIF\";EXT=\"BMP\";EXT=\"WEBP\";EXT=\"AVIF\";EXT=\"JXL\";" \
    "EXT=\"TGA\";EXT=\"PSD\";EXT=\"DJVU\";EXT=\"DJV\""

DCPCALL void ListGetDetectString(char* DetectString, int maxlen) {
    snprintf(DetectString, maxlen - 1, "%s", SUPPORTED_EXTENSIONS);
}

DCPCALL int ListSearchDialog(HWND ListWin, int FindNext) {
    Q_UNUSED(ListWin)
    Q_UNUSED(FindNext)
    return LISTPLUGIN_OK;
}

DCPCALL int ListSendCommand(HWND ListWin, int Command, int Parameter) {
    Q_UNUSED(ListWin)
    Q_UNUSED(Command)
    Q_UNUSED(Parameter)
    return LISTPLUGIN_OK;
}

DCPCALL void ListSetDefaultParams(ListDefaultParamStruct* dps) {
    Q_UNUSED(dps)
}
