#ifndef VIEWER_WIN32_H
#define VIEWER_WIN32_H

#include "viewercontroller.h"

#include <QString>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>

class InfoPanelWin32;

class ViewerWin32 {
public:
    explicit ViewerWin32(HWND hParent);
    ~ViewerWin32();

    bool loadDocument(const QString& path);
    void closeDocument();
    HWND hwnd() const { return m_hwnd; }

    ViewerController* controller() { return m_controller.get(); }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMsg(UINT msg, WPARAM wp, LPARAM lp);

    void onPaint();
    void onSize(int w, int h);
    void onKeyDown(WPARAM wp, bool shift);
    void onMouseWheel(int delta);
    void onVScroll(int code, int pos);
    void onHScroll(int code, int pos);
    void onDragStart(LPARAM lp);
    void onDragMove(LPARAM lp);
    void onDragEnd();
    void updateScrollBars();
    void updateVisiblePage();
    bool needsStripRerender() const;
    void imageToBitmap(const QImage& src);
    void onControllerChanged();
    int maxScrollY() const;

    HWND m_hwnd = nullptr;
    std::unique_ptr<InfoPanelWin32> m_infoPanel;

    std::unique_ptr<ViewerController> m_controller;
    QImage m_currentImage;
    HBITMAP m_hBitmap = nullptr;

    int m_scrollX = 0;
    int m_scrollY = 0;
    int m_wheelRemainder = 0;
    int m_renderedScrollY = 0;
    int m_renderedPageCount = 0;

    bool m_dragging = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
};

#endif // Q_OS_WIN
#endif // VIEWER_WIN32_H
