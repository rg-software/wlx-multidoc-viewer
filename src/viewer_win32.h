#ifndef VIEWER_WIN32_H
#define VIEWER_WIN32_H

#include "viewercontroller.h"

#include <QString>
#include <QVector>
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
    void onControllerChanged();
    void pageJumpContinuous(int delta);

    HBITMAP bitmapForPage(int page);
    void invalidatePageBitmaps();
    void drawPageBitmap(HDC hdc, HBITMAP hbm, int dstX, int dstY, int srcX, int srcY, int w, int h) const;
    int maxScrollX() const;
    int maxScrollY() const;

    HWND m_hwnd = nullptr;
    std::unique_ptr<InfoPanelWin32> m_infoPanel;

    std::unique_ptr<ViewerController> m_controller;

    // Per-page HBITMAP cache (index page-1). Null until first painted; dropped
    // whenever the controller's layout epoch changes.
    QVector<HBITMAP> m_pageBitmaps;
    int m_bitmapEpoch = -1;

    int m_scrollX = 0;
    int m_scrollY = 0;
    int m_wheelRemainder = 0;

    bool m_dragging = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
};

#endif // Q_OS_WIN
#endif // VIEWER_WIN32_H