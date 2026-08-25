#ifndef VIEWER_WIN32_H
#define VIEWER_WIN32_H

#include "viewercontroller.h"
#include "toolbar.h"
#include "sidebar.h"

#include <QString>
#include <QVector>
#include <memory>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class ToolbarWin32;
class SidebarWin32;

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
    void onMouseIdleMove(LPARAM lp);
    void updateScrollBars();
    void updateVisiblePage();
    void onControllerChanged();
    void pageJumpContinuous(int delta);
    void onSelectionStart(int x, int y);
    void onSelectionMove(int x, int y);
    void onSelectionEnd();
    int pageUnderPoint(int x, int y) const;
    QPointF clientToCanvas(int x, int y) const;
    void paintSelectionOverlay(HDC hdc, const RECT& rc, int panelH);
    void paintSearchOverlay(HDC hdc, const RECT& rc, int panelH);

    HBITMAP bitmapForPage(int page);
    void invalidatePageBitmaps();
    void drawPageBitmap(HDC hdc, HBITMAP hbm, int dstX, int dstY, int srcX, int srcY, int w, int h) const;
    int maxScrollX() const;
    int maxScrollY() const;

    // Toolbar / sidebar chrome.
    void layoutChrome();
    void applyScroll(int scrollY);
    void onSidebarToggle();
    void showHideSidebar(bool visible);
    void onToolbarPrint();
    void printCurrentDocument();
    int toolbarHeight() const;
    int sidebarLeft() const;
    int pageAreaTop() const { return toolbarHeight(); }

    HWND m_hwnd = nullptr;
    std::unique_ptr<ToolbarWin32> m_toolbar;
    std::unique_ptr<SidebarWin32> m_sidebar;

    std::unique_ptr<ViewerController> m_controller;
    toolbar::ToolbarPresenter m_toolbarPresenter;
    SidebarPresenter m_sidebarPresenter;
    bool m_sidebarVisible = false;

    // Per-page HBITMAP cache (index page-1). Null until first painted; dropped
    // whenever the controller's layout epoch changes.
    QVector<HBITMAP> m_pageBitmaps;
    int m_bitmapEpoch = -1;

    int m_scrollX = 0;
    int m_scrollY = 0;
    int m_wheelRemainder = 0;

    // Cached semi-transparent selection-overlay surface (rebuilt only on size
    // change) so dragging a selection doesn't churn a DIB every paint.
    HBITMAP m_overlayBitmap = nullptr;
    int m_overlayW = 0;
    int m_overlayH = 0;

    bool m_dragging = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
    int m_hoverX = 0;
    int m_hoverY = 0;
    bool m_selecting = false;
};

#endif // Q_OS_WIN
#endif // VIEWER_WIN32_H