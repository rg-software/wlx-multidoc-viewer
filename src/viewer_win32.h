#ifndef VIEWER_WIN32_H
#define VIEWER_WIN32_H

#include "document.h"

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
    void updateScrollBars();
    void renderCurrentPage();
    void imageToBitmap();

    HWND m_hwnd = nullptr;
    HWND m_hParent = nullptr;
    WNDPROC m_prevProc = nullptr;

    std::unique_ptr<DocumentEngine> m_engine;
    QImage m_currentImage;
    HBITMAP m_hBitmap = nullptr;

    int m_currentPage = 1;
    float m_zoom = 1.0f;
    bool m_fitToWidth = true;

    int m_scrollX = 0;
    int m_scrollY = 0;
};

#endif // Q_OS_WIN
#endif // VIEWER_WIN32_H
