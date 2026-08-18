#ifndef VIEWER_WIN32_H
#define VIEWER_WIN32_H

#include "document.h"
#include "viewerstate.h"

#include <QString>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>

class ViewerWin32 {
public:
    explicit ViewerWin32(HWND hParent);
    ~ViewerWin32();

    bool loadDocument(const QString& path);
    void closeDocument();
    HWND hwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMsg(UINT msg, WPARAM wp, LPARAM lp);

    void onPaint();
    void onSize(int w, int h);
    void onVScroll(int code, int pos);
    void onHScroll(int code, int pos);
    void onKeyDown(WPARAM wp);
    void onChar(WPARAM wp);
    void onMouseWheel(int delta);
    void updateScrollBars();
    void renderCurrentPage();
    void applyFitZoom();
    void imageToBitmap();
    float dpiScale() const;

    HWND m_hwnd = nullptr;
    HWND m_hParent = nullptr;

    std::unique_ptr<DocumentEngine> m_engine;
    ViewerState m_state;

    QImage m_currentImage;
    HBITMAP m_hBitmap = nullptr;

    int m_scrollX = 0;
    int m_scrollY = 0;
};

#endif // Q_OS_WIN
#endif // VIEWER_WIN32_H
