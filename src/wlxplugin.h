#ifndef WLXPLUGIN_H
#define WLXPLUGIN_H

#include <cstdint>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
typedef void* HWND;
typedef void* HANDLE;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#endif

#ifdef _WIN32
#define DCPCALL extern "C" __declspec(dllexport)
#else
#define DCPCALL extern "C" __attribute__((visibility("default")))
#endif
#define LISTPLUGIN_OK 0
#define LISTPLUGIN_ERROR -1

// ShowFlags bits, per the official WLX SDK (ghisler/WLX-SDK src/listplug.h).
#define lcp_wraptext 1
#define lcp_fittowindow 2
#define lcp_ansi 4
#define lcp_ascii 8
#define lcp_variable 12
#define lcp_forceshow 16
#define lcp_fitlargeronly 32
#define lcp_center 64
#define lcp_darkmode 128
#define lcp_darkmodenative 256

// Lister command codes, per the official WLX SDK (ghisler/WLX-SDK src/listplug.h).
#define lc_copy 1
#define lc_newparams 2
#define lc_selectall 3
#define lc_setpercent 4

struct ListDefaultParamStruct {
    int size;
    char DefaultIniName[MAX_PATH];
};

#endif // WLXPLUGIN_H
