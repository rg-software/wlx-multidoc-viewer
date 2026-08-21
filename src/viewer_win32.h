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
    void updateScrollBars();
    void updateVisiblePage();
    void imageToBitmap(const QImage& src);
    void onControllerChanged();
    void ensureInfoPanel();

    HWND m_hwnd = nullptr;
    HWND m_hParent = nullptr;
    std::unique_ptr<InfoPanelWin32> m_infoPanel;

    std::unique_ptr<ViewerController> m_controller;
    QImage m_currentImage;
    HBITMAP m_hBitmap = nullptr;

    int m_scrollX = 0;
    int m_scrollY = 0;
    int m_rotationShortcutPressed = 0;
};

class InfoPanelWin32 {
public:
    explicit InfoPanelWin32(HWND hParent);
    void setController(ViewerController* controller);
    HWND hwnd() const { return m_hwnd; }
    int height() const;
    void onControllerChanged();
    void onSize(int w, int h);

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMsg(UINT msg, WPARAM wp, LPARAM lp);
    void onPaint();

    HWND m_hwnd = nullptr;
    HWND m_hParent = nullptr;
    ViewerController* m_controller = nullptr;
};

#endif // Q_OS_WIN
#endif // VIEWER_WIN32_H
