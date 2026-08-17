#ifndef WLXPLUGIN_H
#define WLXPLUGIN_H

#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#else
typedef void* HWND;
typedef void* HANDLE;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#endif

#define DCPCALL extern "C" __attribute__((visibility("default")))
#define LISTPLUGIN_OK 0
#define LISTPLUGIN_ERROR -1

#define lcp_forceshow 0x100
#define lcp_wraptext 0x200
#define lcp_ansi 0x400
#define lcp_fittowindow 0x800
#define lcp_center 0x1000
#define lcp_fitlargeonly 0x2000
#define lcp_hidewindowtitle 0x4000

struct ListDefaultParamStruct {
    int size;
    char DefaultIniName[MAX_PATH];
};

#endif // WLXPLUGIN_H
