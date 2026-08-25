#include "viewer_win32.h"
#include "viewer_settings.h"
#include "toolbar_win32.h"
#include "sidebar_win32.h"
#include "print_win32.h"

#ifdef Q_OS_WIN

#include <algorithm>
#include <mutex>
#include <deque>
#include <functional>
#include <unordered_map>
#include <cstring>

#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QVector>

#include <windowsx.h>

#define WLX_VIEWER_CLASS L"WLXDocViewer"

namespace {
inline void setClipboardText(const QString& text) {
    if (text.isEmpty())
        return;
    // Native clipboard (works regardless of any Qt app state in the DLL).
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

// UI marshaling for background search/print callbacks: the worker thread
// enqueues a task and posts a custom message; the viewer's wndProc drains the
// queue on its UI thread. Each viewer HWND owns its own mailbox so stale tasks
// never touch a replaced controller.
std::mutex g_marshalMutex;
std::unordered_map<HWND, std::deque<std::function<void()>>> g_marshalQueues;
constexpr UINT WM_PLUGIN_MARSHAL = WM_APP + 1;

void marshalToWnd(HWND hwnd, std::function<void()> task) {
    if (!hwnd)
        return;
    {
        std::lock_guard<std::mutex> lock(g_marshalMutex);
        g_marshalQueues[hwnd].push_back(std::move(task));
    }
    PostMessageW(hwnd, WM_PLUGIN_MARSHAL, 0, 0);
}

constexpr COLORREF kBgColor = 0xE8E8E8; // light page background (toolbar-ish)
constexpr UINT kDefaultDpi = 96;

using viewer_settings::kKeyboardStepPx;
using viewer_settings::kPageMargin;
using viewer_settings::kScrollBarLineStepPx;
using viewer_settings::kWheelStepPx;

// QImage::Format_RGB888 -> DIB (bottom-left origin, BGR byte order).
HBITMAP QImageToBitmap(const QImage& src) {
    if (src.isNull())
        return nullptr;
    QImage img = src.convertToFormat(QImage::Format_RGB888);
    const int w = img.width();
    const int h = img.height();
    if (w <= 0 || h <= 0)
        return nullptr;

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bi),
                                   DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm || !bits)
        return hbm;

    const int dibStride = ((w * 3 + 3) / 4) * 4;
    const int srcStride = img.bytesPerLine();
    const uchar* srcd = img.constBits();
    auto* dst = static_cast<uchar*>(bits);
    for (int y = 0; y < h; ++y) {
        const uchar* srcRow = srcd + y * srcStride;
        uchar* dstRow = dst + y * dibStride;
        for (int x = 0; x < w; ++x) {
            uchar r = srcRow[x * 3 + 0];
            uchar g = srcRow[x * 3 + 1];
            uchar b = srcRow[x * 3 + 2];
            dstRow[x * 3 + 0] = b;
            dstRow[x * 3 + 1] = g;
            dstRow[x * 3 + 2] = r;
        }
    }
    return hbm;
}
} // namespace

ViewerWin32::ViewerWin32(HWND hParent) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WLX_VIEWER_CLASS;

    static bool registered = false;
    if (!registered) {
        RegisterClassExW(&wc);
        registered = true;
    }

    m_hwnd = CreateWindowExW(
        0, WLX_VIEWER_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
        0, 0, 0, 0,
        hParent, nullptr, hInst, this);

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    m_controller = std::make_unique<ViewerController>();
    m_controller->setStateChangedCallback([this]() { onControllerChanged(); });
    m_controller->setDpiScale(static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi);
    m_controller->setUiMarshal([this](std::function<void()> task) {
        marshalToWnd(m_hwnd, std::move(task));
    });

    m_toolbar = std::make_unique<ToolbarWin32>(m_hwnd);
    m_toolbar->setDpiScale(static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi);
    m_toolbarPresenter.attach(m_controller.get(), m_toolbar.get());
    m_toolbarPresenter.setScrollApplier([this](int scrollY) { applyScroll(scrollY); });
    m_toolbarPresenter.sidebarToggleHandler = [this]() { onSidebarToggle(); };
    m_toolbarPresenter.printHandler = [this]() { onToolbarPrint(); };

    m_sidebar = std::make_unique<SidebarWin32>(m_hwnd);
    m_sidebar->setDpiScale(static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi);
    m_sidebarPresenter.attach(m_controller.get(), m_sidebar.get());
    m_sidebarPresenter.setScrollApplier([this](int scrollY) { applyScroll(scrollY); });
    m_toolbarPresenter.sidebarAvailable = [this]() { return m_sidebarPresenter.hasOutline(); };
    m_toolbarPresenter.copyHandler = [this](const QString& text) { setClipboardText(text); };
    m_toolbarPresenter.sidebarVisible = [this]() { return m_sidebarVisible; };
    m_toolbarPresenter.refreshState();
}

ViewerWin32::~ViewerWin32() {
    closeDocument();
    if (m_overlayBitmap) {
        DeleteObject(m_overlayBitmap);
        m_overlayBitmap = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(g_marshalMutex);
        g_marshalQueues.erase(m_hwnd);
    }
    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

bool ViewerWin32::loadDocument(const QString& path) {
    closeDocument();
    m_controller->setEngine(createEngine(path));
    if (!m_controller->openDocument(path))
        return false;
    m_scrollX = 0;
    m_scrollY = 0;
    m_sidebarPresenter.reload();
    showHideSidebar(false);
    m_toolbar->setChecked(toolbar::Control::SidebarToggle, false);
    onControllerChanged();
    // reload() runs after openDocument() (which already fired refreshState),
    // so re-sync the toolbar now that sidebarAvailable()/hasOutline() are real.
    m_toolbarPresenter.refreshState();
    return true;
}

void ViewerWin32::closeDocument() {
    invalidatePageBitmaps();
    if (m_controller)
        m_controller->closeDocument();
    m_scrollX = 0;
    m_scrollY = 0;
    m_sidebarPresenter.reload();
    showHideSidebar(false);
    if (m_sidebar)
        m_sidebar->setVisible(false);
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void ViewerWin32::onControllerChanged() {
    if (!m_controller)
        return;

    // While a text selection is being dragged the only thing that changes is
    // the highlight overlay. Repaint just the page area (never the toolbar)
    // so selection updates don't flicker the chrome. Still refresh
    // the toolbar so Copy's enabled state tracks the selection (it is idempotent
    // thanks to the per-control change guards).
    if (m_selecting) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        RECT pa = {
            static_cast<LONG>(sidebarLeft()),
            static_cast<LONG>(pageAreaTop()),
            rc.right,
            rc.bottom - 0
        };
        InvalidateRect(m_hwnd, &pa, FALSE);
        if (m_toolbar)
            m_toolbarPresenter.refreshState();
        return;
    }

    m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
    m_scrollY = (std::clamp)(m_scrollY, 0, maxScrollY());
    m_controller->setScrollAnchor(m_scrollY);
    updateScrollBars();
    m_toolbarPresenter.refreshState();
    m_sidebarPresenter.onPageChanged(m_controller->currentPage());

    // A search that found matches wants the first match brought into view.
    if (m_controller->hasPendingSearchJump()) {
        m_scrollY = (std::clamp)(m_controller->takeSearchJump(), 0, maxScrollY());
        m_scrollX = 0;
        updateVisiblePage();
        updateScrollBars();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// Toolbar / sidebar chrome
// ---------------------------------------------------------------------------

int ViewerWin32::toolbarHeight() const {
    return m_toolbar ? m_toolbar->heightPx() : 0;
}

int ViewerWin32::sidebarLeft() const {
    return (m_sidebarVisible && m_sidebar) ? m_sidebar->widthPx() : 0;
}

void ViewerWin32::layoutChrome() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int w = static_cast<int>(rc.right);
    const int h = static_cast<int>(rc.bottom);

    if (m_toolbar && m_toolbar->hwnd())
        MoveWindow(m_toolbar->hwnd(), 0, 0, w, toolbarHeight(), TRUE);
    if (m_sidebar && m_sidebar->hwnd()) {
        const int sw = m_sidebar->widthPx();
        MoveWindow(m_sidebar->hwnd(), 0, toolbarHeight(), sw,
                   (std::max)(0, h - toolbarHeight()), TRUE);
    }
}

void ViewerWin32::showHideSidebar(bool visible) {
    m_sidebarVisible = visible;
    if (m_sidebar)
        m_sidebar->setVisible(visible);
    layoutChrome();
    if (m_controller) {
        m_controller->setLeftChrome(sidebarLeft());
        m_scrollY = m_controller->relayout(m_scrollY);
        m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
        m_scrollY = (std::clamp)(m_scrollY, 0, maxScrollY());
        onControllerChanged();
    }
}

void ViewerWin32::onSidebarToggle() {
    if (!m_sidebarPresenter.hasOutline() || !m_controller || !m_controller->hasDocument())
        return;
    const bool now = !m_sidebarVisible;
    showHideSidebar(now);
    if (m_toolbar)
        m_toolbar->setChecked(toolbar::Control::SidebarToggle, now);
}

void ViewerWin32::applyScroll(int scrollY) {
    if (!m_controller || !m_controller->hasDocument())
        return;
    m_scrollY = (std::clamp)(scrollY, 0, maxScrollY());
    m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
    m_controller->setScrollAnchor(m_scrollY);
    updateVisiblePage();
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ViewerWin32::onToolbarPrint() {
    printCurrentDocument();
}

// Print backend lives in print_win32.cpp (tasks 10.1/11.x): opens the native
// dialog and spools the chosen range at printer resolution.
void ViewerWin32::printCurrentDocument() {
    if (m_controller)
        printDocumentWin32(m_hwnd, m_controller.get());
}

LRESULT CALLBACK ViewerWin32::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<ViewerWin32*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self)
        return self->handleMsg(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT ViewerWin32::handleMsg(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        onPaint();
        return 0;
    case WM_SIZE:
        onSize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_VSCROLL:
        onVScroll(LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_HSCROLL:
        onHScroll(LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_KEYDOWN:
        onKeyDown(wp, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        return 0;
    case WM_PLUGIN_MARSHAL: {
        // Run every task posted by a background worker on this UI thread.
        std::deque<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lock(g_marshalMutex);
            auto it = g_marshalQueues.find(m_hwnd);
            if (it != g_marshalQueues.end())
                tasks.swap(it->second);
        }
        for (auto& task : tasks)
            task();
        return 0;
    }
    case WM_DPICHANGED: {
        const float dpi = static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi;
        if (m_toolbar) m_toolbar->setDpiScale(dpi);
        if (m_sidebar) m_sidebar->setDpiScale(dpi);
        RECT rc;
        if (GetClientRect(m_hwnd, &rc))
            onSize(static_cast<int>(rc.right), static_cast<int>(rc.bottom));
        return 0;
    }
    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_COPY:
        // Standard copy message (some hosts send this instead of a hotkey).
        if (m_controller && m_controller->hasSelection()) {
            const QString text = m_controller->selectedText();
            if (!text.isEmpty())
                setClipboardText(text);
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: // CS_DBLCLKS class: a fast second press must still drag
        // Clicking the reading area takes over keyboard focus from the TOC/
        // toolbar so page hotkeys (arrows, PageUp/Down, Esc) work again.
        if (GetFocus() != m_hwnd)
            SetFocus(m_hwnd);
        // Branch: press on selectable text (within the hit tolerance) starts a
        // char-level text selection; empty page area falls through to the pan
        // gesture so users can still drag the page around (SumatraPDF-style).
        if (m_controller && m_controller->hasDocument()) {
            const int page = pageUnderPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            const QPointF canvasPt = clientToCanvas(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            const int word = (page >= 1)
                ? m_controller->wordAtCanvas(page, canvasPt, viewer_settings::kSelectionHitTolerancePx)
                : -1;
            if (word >= 0 && m_controller->pageHasText(page)) {
                onSelectionStart(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                if (m_selecting)
                    return 0;
            }
        }
        onDragStart(lp);
        if (!m_dragging)
            break;
        return 0;
    case WM_MOUSEMOVE:
        if (m_selecting) {
            onSelectionMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        }
        if (!m_dragging) {
            onMouseIdleMove(lp);
            break;
        }
        onDragMove(lp);
        return 0;
    case WM_LBUTTONUP:
        if (m_selecting) {
            onSelectionEnd();
            return 0;
        }
        if (!m_dragging)
            break;
        onDragEnd();
        return 0;
    case WM_SETCURSOR:
        if (m_dragging && LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        if (m_selecting && LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursor(nullptr, IDC_IBEAM));
            return TRUE;
        }
        // Hover: I-beam only when the pointer is actually over selectable text
        // (hit tolerance); empty page areas keep the arrow so users know they
        // can drag.
        if (!m_dragging && !m_selecting && LOWORD(lp) == HTCLIENT &&
            m_controller && m_controller->hasDocument()) {
            const int page = (std::max)(1, pageUnderPoint(m_hoverX, m_hoverY));
            if (m_controller->pageHasText(page)) {
                const QPointF canvasPt = clientToCanvas(m_hoverX, m_hoverY);
                if (m_controller->wordAtCanvas(page, canvasPt, viewer_settings::kSelectionHitTolerancePx) >= 0) {
                    SetCursor(LoadCursor(nullptr, IDC_IBEAM));
                    return TRUE;
                }
            }
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

void ViewerWin32::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int w = static_cast<int>(rc.right);
    const int h = static_cast<int>(rc.bottom);

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ hOldBmp = SelectObject(hdcMem, hbmMem);

    HBRUSH bgBrush = CreateSolidBrush(kBgColor);
    FillRect(hdcMem, &rc, bgBrush);
    DeleteObject(bgBrush);

    const int top = pageAreaTop();
    const int left = sidebarLeft();
    const int vw = (std::max)(1, w - left);
    const int vh = (std::max)(1, h - top);

    const bool paged = !m_controller || m_controller->isPagedMode();
    if (paged) {
        if (m_controller && m_controller->hasDocument()) {
            if (vh > 0) {
                QRect r = m_controller->pageRect(m_controller->currentPage());
                int imgW = r.isValid() ? r.width() : 0;
                int imgH = r.isValid() ? r.height() : 0;
                if (imgW > 0 && imgH > 0) {
                    // Center the page only when it fits; otherwise scroll the
                    // visible part with m_scrollX/m_scrollY offset.
                    int dstX = left + ((imgW <= vw) ? (std::max)(0, (vw - imgW) / 2) : -m_scrollX);
                    int dstY = top + ((imgH <= vh) ? (std::max)(0, (vh - imgH) / 2) : -m_scrollY);
                    drawPageBitmap(hdcMem, bitmapForPage(m_controller->currentPage()),
                                   dstX, dstY, 0, 0, imgW, imgH);
                }
            }
        }
    } else {
        const QSize cs = m_controller->contentSize();
        const int cx = (std::max)(0, (vw - cs.width()) / 2);
        const int cy = (std::max)(0, (vh - cs.height()) / 2);

        const int firstVisible = m_controller->firstPageAtScroll(m_scrollY);
        for (int page = firstVisible; page <= m_controller->pageCount(); ++page) {
            const QRect r = m_controller->pageRect(page);
            if (r.bottom() < m_scrollY)
                continue;
            if (r.top() > m_scrollY + vh)
                break;
            const int dstX = left + cx + r.x() - m_scrollX;
            const int dstY = top + cy + (r.y() - m_scrollY);
            if (dstX >= w || dstY >= h)
                continue;
            drawPageBitmap(hdcMem, bitmapForPage(page), dstX, dstY, 0, 0, r.width(), r.height());
        }
        m_controller->trimRenderCache(m_scrollY);
    }

    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

    // Selection + search match overlays drawn through ONE translucent surface so
    // both use the identical light-yellow and avoid a second blit/paint.
    if (m_controller &&
        (m_controller->hasSelection() || m_controller->hasSearchHighlights()))
        paintSelectionOverlay(hdc, rc, top);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);

    EndPaint(m_hwnd, &ps);
}

void ViewerWin32::drawPageBitmap(HDC hdc, HBITMAP hbm, int dstX, int dstY, int srcX, int srcY, int w, int h) const {
    if (!hbm || w <= 0 || h <= 0)
        return;
    HDC hdcBmp = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(hdcBmp, hbm);
    // Shift source when the destination would start off-window left/top so the
    // visible part of the page lines up.
    if (dstX < 0) { srcX += -dstX; w += dstX; dstX = 0; }
    if (dstY < 0) { srcY += -dstY; h += dstY; dstY = 0; }
    if (w > 0 && h > 0)
        BitBlt(hdc, dstX, dstY, w, h, hdcBmp, srcX, srcY, SRCCOPY);
    SelectObject(hdcBmp, old);
    DeleteDC(hdcBmp);
}

HBITMAP ViewerWin32::bitmapForPage(int page) {
    if (!m_controller || !m_controller->hasDocument() || page < 1 || page > m_controller->pageCount())
        return nullptr;
    if (m_bitmapEpoch != m_controller->layoutEpoch())
        invalidatePageBitmaps();
    if (m_pageBitmaps.isEmpty())
        m_pageBitmaps.resize(m_controller->pageCount());
    if (m_pageBitmaps[page - 1])
        return m_pageBitmaps[page - 1];

    QImage img = m_controller->renderPageCached(page);
    if (img.isNull())
        return nullptr;
    m_pageBitmaps[page - 1] = QImageToBitmap(img);
    return m_pageBitmaps[page - 1];
}

void ViewerWin32::invalidatePageBitmaps() {
    for (HBITMAP hbm : m_pageBitmaps) {
        if (hbm)
            DeleteObject(hbm);
    }
    m_pageBitmaps.clear();
    m_bitmapEpoch = m_controller ? m_controller->layoutEpoch() : -1;
}

int ViewerWin32::maxScrollX() const {
    if (!m_controller || !m_controller->hasDocument())
        return 0;
    if (m_controller->isPagedMode())
        return m_controller->maxScrollOffsetXForPage(m_controller->currentPage());
    return m_controller->maxScrollOffsetX();
}

int ViewerWin32::maxScrollY() const {
    if (!m_controller || !m_controller->hasDocument())
        return 0;
    if (m_controller->isPagedMode())
        return m_controller->maxScrollOffsetYForPage(m_controller->currentPage());
    return m_controller->maxScrollOffset();
}

void ViewerWin32::onSize(int w, int h) {
    const float dpi = m_hwnd ? static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi : 1.0f;
    if (m_toolbar)
        m_toolbar->setDpiScale(dpi);
    if (m_sidebar)
        m_sidebar->setDpiScale(dpi);
    layoutChrome();
    if (m_controller) {
        m_controller->setDpiScale(dpi);
        m_controller->setViewportSize(QSize(w, h));
        m_controller->setTopChrome(toolbarHeight());
        m_controller->setBottomChrome(0);
        m_controller->setLeftChrome(sidebarLeft());
        m_scrollY = m_controller->relayout(m_scrollY);
        m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
        m_scrollY = (std::clamp)(m_scrollY, 0, maxScrollY());
        onControllerChanged();
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void ViewerWin32::pageJumpContinuous(int delta) {
    if (!m_controller)
        return;
    const int out = (std::clamp)(m_controller->pageAtScrollOffset(m_scrollY) + delta, 1, m_controller->pageCount());
    m_scrollY = m_controller->scrollOffsetForPage(out);
    m_scrollX = 0;
    m_scrollY = (std::clamp)(m_scrollY, 0, m_controller->maxScrollOffset());
}

void ViewerWin32::onKeyDown(WPARAM wp, bool shift) {
    // Focus neutrality (design D8): typed characters belong to a focused
    // toolbar edit box and must never trigger viewer shortcuts.
    if (m_toolbar && m_toolbar->isEditFocused())
        return;
    if (!m_controller || !m_controller->hasDocument())
        return;

    bool captured = false;
    bool continuous = !m_controller->isPagedMode();
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    switch (wp) {
    case VK_RIGHT:
    case VK_NEXT:
        if (continuous)
            pageJumpContinuous(+1);
        else
            m_controller->nextPage();
        captured = true;
        break;
    case VK_LEFT:
    case VK_PRIOR:
        if (continuous)
            pageJumpContinuous(-1);
        else
            m_controller->prevPage();
        captured = true;
        break;
    case VK_UP:
        if (continuous) {
            m_scrollY -= kKeyboardStepPx;
        } else {
            m_controller->prevPage();
        }
        captured = true;
        break;
    case VK_DOWN:
        if (continuous) {
            m_scrollY += kKeyboardStepPx;
        } else {
            m_controller->nextPage();
        }
        captured = true;
        break;
    case VK_HOME:
        if (continuous)
            m_scrollY = 0;
        else
            m_controller->firstPage();
        captured = true;
        break;
    case VK_END:
        if (continuous)
            m_scrollY = m_controller->maxScrollOffset();
        else
            m_controller->lastPage();
        captured = true;
        break;
    case 'V':
        if (shift) {
            // Shift+V = cycle fit mode (F is a host hotkey on TC/DC, so fit
            // cycling uses Shift+V; V alone toggles paged/continuous).
            m_scrollY = m_controller->cycleFitMode(m_scrollY);
            captured = true;
            break;
        }
        m_controller->toggleMode();
        // The toggle flips the mode, which can invalidate the `continuous`
        // captured at the top of onKeyDown; refresh it so scroll positioning
        // uses the new mode.
        continuous = !m_controller->isPagedMode();
        if (continuous) {
            m_scrollX = 0;
            m_scrollY = m_controller->scrollOffsetForPage(m_controller->currentPage());
        } else {
            m_scrollX = 0;
            m_scrollY = 0;
        }
        captured = true;
        break;
    case 'C':
        if (ctrl) {
            if (m_controller && m_controller->hasSelection()) {
                const QString text = m_controller->selectedText();
                if (!text.isEmpty())
                    setClipboardText(text);
            }
            captured = true;
        }
        break;
    case VK_INSERT:
        // Ctrl+Ins copies the selection (standard shortcut).
        if (ctrl) {
            if (m_controller && m_controller->hasSelection()) {
                const QString text = m_controller->selectedText();
                if (!text.isEmpty())
                    setClipboardText(text);
            }
            captured = true;
        }
        break;
    case VK_ESCAPE:
        if (m_selecting)
            onSelectionEnd();
        if (m_controller)
            m_controller->clearSelection();
        captured = true;
        break;
    case 'R':
        if (shift)
            m_scrollY = m_controller->rotateCcw(m_scrollY);
        else
            m_scrollY = m_controller->rotateCw(m_scrollY);
        captured = true;
        break;
    case 0xBB:
    case 0x6B:
        m_scrollY = m_controller->zoomIn(m_scrollY);
        captured = true;
        break;
    case 0xBD:
    case 0x6D:
        m_scrollY = m_controller->zoomOut(m_scrollY);
        captured = true;
        break;
    case '0':
        m_scrollY = m_controller->setManualZoom(1.0f, m_scrollY);
        captured = true;
        break;
    }

    if (captured) {
        if (continuous) {
            m_scrollY = (std::clamp)(m_scrollY, 0, m_controller->maxScrollOffset());
            m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
            updateVisiblePage();
            updateScrollBars();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        } else if (wp == VK_RIGHT || wp == VK_LEFT || wp == VK_NEXT || wp == VK_PRIOR
                   || wp == VK_HOME || wp == VK_END || wp == 'V' || wp == 'R'
                   || wp == 0xBB || wp == 0x6B || wp == 0xBD || wp == 0x6D || wp == '0') {
            m_scrollX = 0;
            m_scrollY = 0;
        }
    }
}

void ViewerWin32::onDragStart(LPARAM lp) {
    if (!m_controller || !m_controller->hasDocument())
        return;
    // Panning is available when the content overflows the viewport in at least
    // one axis. In paged mode the page may overflow horizontally (page wider
    // than window) and/or vertically (page taller than window); both are
    // drag-pannable. Continuous mode always allows drag.
    const bool paged = m_controller->isPagedMode();
    const int xRange = maxScrollX();
    const int yRange = maxScrollY();
    if (paged && xRange <= 0 && yRange <= 0)
        return;
    m_dragging = true;
    m_lastMouseX = GET_X_LPARAM(lp);
    m_lastMouseY = GET_Y_LPARAM(lp);
    SetCapture(m_hwnd);
    SetCursor(LoadCursor(nullptr, IDC_HAND));
}

void ViewerWin32::onDragMove(LPARAM lp) {
    if (!m_dragging)
        return;
    const int x = GET_X_LPARAM(lp);
    const int y = GET_Y_LPARAM(lp);
    const int dx = x - m_lastMouseX;
    const int dy = y - m_lastMouseY;
    m_lastMouseX = x;
    m_lastMouseY = y;

    if (m_controller && m_controller->isPagedMode()) {
        // Paged mode: pan within the current page's overflow in both axes.
        m_scrollX -= dx;
        m_scrollY -= dy;
        m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
        m_scrollY = (std::clamp)(m_scrollY, 0, maxScrollY());
        updateVisiblePage();
    } else {
        m_scrollX -= dx;
        m_scrollY -= dy;
        m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
        m_scrollY = (std::clamp)(m_scrollY, 0, m_controller->maxScrollOffset());
        updateVisiblePage();
    }
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ViewerWin32::onDragEnd() {
    if (!m_dragging)
        return;
    m_dragging = false;
    ReleaseCapture();
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    updateScrollBars();
}

// ---------------------------------------------------------------------------
// Text selection
// ---------------------------------------------------------------------------

int ViewerWin32::pageUnderPoint(int x, int y) const {
    if (!m_controller || m_controller->isPagedMode()) {
        return m_controller ? m_controller->currentPage() : 1;
    }
    const QPointF cpt = clientToCanvas(x, y);
    for (int page = 1; page <= m_controller->pageCount(); ++page) {
        if (m_controller->pageRect(page).contains(cpt.toPoint()))
            return page;
    }
    return -1;
}

QPointF ViewerWin32::clientToCanvas(int x, int y) const {
    // Mirror the paint math: content is centered in the viewport when smaller,
    // otherwise shifted by scroll. In paged mode the current page is centered
    // when it fits; otherwise it starts at -scroll.
    const int top = pageAreaTop();
    const int left = sidebarLeft();
    RECT cr;
    GetClientRect(m_hwnd, &cr);
    const double viewW = cr.right - left;
    const double viewH = (cr.bottom - top);

    if (m_controller->isPagedMode()) {
        const QRect r = m_controller->pageRect(m_controller->currentPage());
        const int imgW = r.isValid() ? r.width() : 0;
        const int imgH = r.isValid() ? r.height() : 0;
        const double dstX = left + ((imgW <= viewW) ? std::max(0, (int)((viewW - imgW) / 2)) : -m_scrollX);
        const double dstY = top + ((imgH <= viewH) ? std::max(0, (int)((viewH - imgH) / 2)) : -m_scrollY);
        // Page-local canvas: the on-screen page starts at (dstX,dstY), but the
        // controller's transform places it at pageRect.topLeft(). Shift so the
        // returned canvas point is in the same space as pageRect.
        return QPointF(x - dstX + r.x(), y - dstY + r.y());
    }

    const QSize cs = m_controller->contentSize();
    const double cx = std::max(0, (int)((viewW - cs.width()) / 2));
    const double cy = std::max(0, (int)((viewH - cs.height()) / 2));
    return QPointF(x - left - cx + m_scrollX, (y - top) - cy + m_scrollY);
}

void ViewerWin32::onMouseIdleMove(LPARAM lp) {
    if (m_dragging || m_selecting)
        return;
    m_hoverX = GET_X_LPARAM(lp);
    m_hoverY = GET_Y_LPARAM(lp);
}

void ViewerWin32::onSelectionStart(int x, int y) {
    if (!m_controller)
        return;
    const int page = pageUnderPoint(x, y);
    if (page < 1)
        return;
    const QPointF cpt = clientToCanvas(x, y);
    const int word = m_controller->wordAtCanvas(page, cpt, viewer_settings::kSelectionHitTolerancePx);
    if (word < 0)
        return; // empty area -> stays in pan mode
    const int ch = m_controller->charAtCanvas(page, word, cpt);
    m_controller->clearSelection();
    m_selecting = true;
    m_controller->beginSelection(page, word, ch);
    SetCapture(m_hwnd);
    SetCursor(LoadCursor(nullptr, IDC_IBEAM));
}

void ViewerWin32::onSelectionMove(int x, int y) {
    if (!m_selecting)
        return;
    const int page = pageUnderPoint(x, y);
    if (page < 1)
        return;
    const QPointF canvas = clientToCanvas(x, y);
    const int word = m_controller->wordAtCanvas(page, canvas); // nearest while dragging
    if (word < 0)
        return;
    const int ch = m_controller->charAtCanvas(page, word, canvas);
    m_controller->updateSelection(page, word, ch);
}

void ViewerWin32::onSelectionEnd() {
    if (!m_selecting)
        return;
    m_selecting = false;
    ReleaseCapture();
    m_controller->endSelection();
    InvalidateRect(m_hwnd, nullptr, FALSE);
    // The cursor was pinned to an I-beam; restore it to whatever the pointer is
    // over now (normally the arrow, since selection release is outside text).
    const POINT pt = { m_hoverX, m_hoverY };
    RECT cr;
    GetClientRect(m_hwnd, &cr);
    if (PtInRect(&cr, pt)) {
        const int page = (std::max)(1, pageUnderPoint(m_hoverX, m_hoverY));
        QPointF canvasPt = clientToCanvas(m_hoverX, m_hoverY);
        const bool overText = m_controller && page <= m_controller->pageCount() &&
                              m_controller->pageHasText(page) &&
                              m_controller->wordAtCanvas(page, canvasPt,
                                 viewer_settings::kSelectionHitTolerancePx) >= 0;
        SetCursor(LoadCursor(nullptr, overText ? IDC_IBEAM : IDC_ARROW));
    } else {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
}

void ViewerWin32::paintSelectionOverlay(HDC hdc, const RECT& rc, int topChrome) {
    // Runs for text-selection and/or search-match highlights. The inner loops
    // already gate per-mode, so do NOT bail out when only search exists.
    if (!m_controller || !hdc)
        return;

    const int w = static_cast<int>(rc.right - rc.left);
    const int hgt = static_cast<int>(rc.bottom - rc.top);
    if (w <= 0 || hgt <= 0)
        return;

    // Cache the overlay surface; recreate only when the client size changes so
    // dragging a selection doesn't allocate + destroy a DIB every WM_PAINT.
    if (!m_overlayBitmap || m_overlayW != w || m_overlayH != hgt) {
        if (m_overlayBitmap) {
            DeleteObject(m_overlayBitmap);
            m_overlayBitmap = nullptr;
        }
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -hgt;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        m_overlayBitmap = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!m_overlayBitmap || !bits) {
            if (m_overlayBitmap) {
                DeleteObject(m_overlayBitmap);
                m_overlayBitmap = nullptr;
            }
            return;
        }
        m_overlayW = w;
        m_overlayH = hgt;
    }

    BITMAP biInfo;
    if (!GetObjectW(m_overlayBitmap, sizeof(biInfo), &biInfo) || !biInfo.bmBits)
        return;
    auto* bits = static_cast<uchar*>(biInfo.bmBits);
    const int stride = ((w * 4 + 3) / 4) * 4;
    memset(bits, 0, static_cast<size_t>(stride) * hgt);

    // Light yellow, ~40% alpha (premultiplied into the DIB); active matches use
    // semi-transparent cyan below.
    constexpr BYTE kAr = 255, kAg = 240, kAb = 105, kAa = 105;
    constexpr BYTE kCr = 0, kCg = 220, kCb = 220, kCa = 150;
    auto fillOverlayC = [&](const RECT& r, BYTE rr, BYTE gg, BYTE bb, BYTE aa) {
        const int x = (std::max)(0L, static_cast<long>(r.left));
        const int y = (std::max)(0L, static_cast<long>(r.top));
        const int rw = (std::min)(r.right, static_cast<long>(w)) - x;
        const int rh = (std::min)(r.bottom, static_cast<long>(hgt)) - y;
        if (rw <= 0 || rh <= 0)
            return;
        for (int yy = 0; yy < rh; ++yy) {
            auto* row = bits + static_cast<size_t>(y + yy) * stride;
            for (int xx = 0; xx < rw; ++xx) {
                auto* px = row + static_cast<size_t>(x + xx) * 4;
                px[0] = static_cast<BYTE>((bb * aa) / 255);
                px[1] = static_cast<BYTE>((gg * aa) / 255);
                px[2] = static_cast<BYTE>((rr * aa) / 255);
                px[3] = aa;
            }
        }
    };
    auto fillOverlay = [&](const RECT& r) {
        fillOverlayC(r, kAr, kAg, kAb, kAa);
    };

    const int left = sidebarLeft();
    const int vw = (std::max)(1, w - left);
    const int vh = (std::max)(1, hgt - topChrome);
    const QSize cs = m_controller->contentSize();
    const bool paged = m_controller->isPagedMode();

    // Rects are canvas-space; convert to client by mirroring onPaint placement
    // (canvas pageRect -> on-screen), then draw into the overlay surface.
    auto pageOrigin = [&](const QRect& pr) -> QPointF {
        if (paged) {
            const int imgW = pr.width();
            const int imgH = pr.height();
            return QPointF(left + ((imgW <= vw) ? std::max(0, (vw - imgW) / 2) : -m_scrollX),
                           topChrome + ((imgH <= vh) ? std::max(0, (vh - imgH) / 2) : -m_scrollY));
        }
        const int cx = std::max(0, (vw - cs.width()) / 2);
        const int cy = std::max(0, (vh - cs.height()) / 2);
        return QPointF(left + cx + pr.x() - m_scrollX,
                       topChrome + cy + pr.y() - m_scrollY);
    };
    auto addClientSpan = [&](const QRectF& r, const QRect& pr, const QPointF& org,
                              BYTE rr, BYTE gg, BYTE bb, BYTE aa) {
        RECT ov;
        ov.left = (LONG)(r.x() - pr.x() + org.x());
        ov.top = (LONG)(r.y() - pr.y() + org.y());
        ov.right = (LONG)(ov.left + r.width()) + 1;
        ov.bottom = (LONG)(ov.top + r.height()) + 1;
        fillOverlayC(ov, rr, gg, bb, aa);
    };
    auto addSpanYellow = [&](const QRectF& r, const QRect& pr, const QPointF& org) {
        addClientSpan(r, pr, org, kAr, kAg, kAb, kAa);
    };

    for (int page = (paged ? m_controller->currentPage() : 1);
         page <= (paged ? m_controller->currentPage() : m_controller->pageCount()); ++page) {
        const QRect pr = m_controller->pageRect(page);
        if (!pr.isValid())
            continue;
        const QPointF origin = pageOrigin(pr);

        // Text selection rects.
        if (m_controller->hasSelection()) {
            const QVector<QRectF> rects = m_controller->highlightRects(page);
            for (const QRectF& r : rects)
                addSpanYellow(r, pr, origin);
        }
        // Search match rects (all matches yellow; the active one is cyan so it
        // stands out clearly).
        if (m_controller->hasSearchHighlights()) {
            const QVector<QRectF> rects = m_controller->searchRectsOnPage(page);
            for (const QRectF& r : rects)
                addSpanYellow(r, pr, origin);
            const QRectF active = m_controller->activeSearchRectOnPage(page);
            if (!active.isNull())
                addClientSpan(active, pr, origin, kCr, kCg, kCb, kCa);
        }
    }

    HDC mem = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mem, m_overlayBitmap);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, 0, 0, w, hgt, mem, 0, 0, w, hgt, bf);
    SelectObject(mem, old);
    DeleteDC(mem);
}

void ViewerWin32::paintSearchOverlay(HDC hdc, const RECT& rc, int panelH) {
    Q_UNUSED(hdc)
    Q_UNUSED(rc)
    Q_UNUSED(panelH)
}

void ViewerWin32::onMouseWheel(int delta) {
    if (!m_controller)
        return;
    if (m_controller->isPagedMode()) {
        if (delta < 0)
            m_controller->nextPage();
        else
            m_controller->prevPage();
        updateScrollBars();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    // Accumulate fractional wheel deltas so high-resolution wheels and
    // trackpads (|delta| < WHEEL_DELTA per message) scroll smoothly.
    m_wheelRemainder += delta * kWheelStepPx;
    const int applied = m_wheelRemainder / WHEEL_DELTA;
    m_wheelRemainder -= applied * WHEEL_DELTA;
    m_scrollY -= applied;
    m_scrollY = (std::clamp)(m_scrollY, 0, m_controller->maxScrollOffset());
    updateVisiblePage();
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ViewerWin32::onVScroll(int code, int pos) {
    if (!m_controller)
        return;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(m_hwnd, SB_VERT, &si);

    if (m_controller->isPagedMode()) {
        bool changed = false;
        switch (code) {
        case SB_LINEDOWN: case SB_PAGEDOWN:
            changed = m_controller->nextPage();
            break;
        case SB_LINEUP: case SB_PAGEUP:
            changed = m_controller->prevPage();
            break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION:
            changed = m_controller->goToPage(pos + 1);
            break;
        case SB_TOP:
            changed = m_controller->firstPage();
            break;
        case SB_BOTTOM:
            changed = m_controller->lastPage();
            break;
        }
        if (changed) {
            m_scrollX = 0;
            m_scrollY = 0;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }

    switch (code) {
    case SB_LINEUP:    m_scrollY -= kScrollBarLineStepPx; break;
    case SB_LINEDOWN:  m_scrollY += kScrollBarLineStepPx; break;
    case SB_PAGEUP:    m_scrollY -= si.nPage; break;
    case SB_PAGEDOWN:  m_scrollY += si.nPage; break;
    case SB_THUMBTRACK: {
        // The thumb position is conveyed 16-bit in WM_VSCROLL's HIWORD, which
        // truncates ranges > 65535; read the true 32-bit track position.
        SCROLLINFO tr = {};
        tr.cbSize = sizeof(tr);
        tr.fMask = SIF_TRACKPOS;
        if (GetScrollInfo(m_hwnd, SB_VERT, &tr) && tr.nTrackPos >= 0)
            m_scrollY = (int)tr.nTrackPos;
        else
            m_scrollY = pos;
        break;
    }
    case SB_THUMBPOSITION: {
        SCROLLINFO siP = {};
        siP.cbSize = sizeof(siP);
        siP.fMask = SIF_POS;
        if (GetScrollInfo(m_hwnd, SB_VERT, &siP))
            m_scrollY = (int)siP.nPos;
        else
            m_scrollY = pos;
        break;
    }
    case SB_TOP:       m_scrollY = 0; break;
    case SB_BOTTOM:    m_scrollY = m_controller->maxScrollOffset(); break;
    }
    m_scrollY = (std::clamp)(m_scrollY, 0, m_controller->maxScrollOffset());
    updateVisiblePage();
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ViewerWin32::onHScroll(int code, int pos) {
    if (!m_controller)
        return;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(m_hwnd, SB_HORZ, &si);

    switch (code) {
    case SB_LINELEFT:   m_scrollX -= kScrollBarLineStepPx; break;
    case SB_LINERIGHT:  m_scrollX += kScrollBarLineStepPx; break;
    case SB_PAGELEFT:   m_scrollX -= si.nPage; break;
    case SB_PAGERIGHT:  m_scrollX += si.nPage; break;
    case SB_THUMBTRACK: {
        SCROLLINFO tr = {};
        tr.cbSize = sizeof(tr);
        tr.fMask = SIF_TRACKPOS;
        if (GetScrollInfo(m_hwnd, SB_HORZ, &tr) && tr.nTrackPos >= 0)
            m_scrollX = (int)tr.nTrackPos;
        else
            m_scrollX = pos;
        break;
    }
    case SB_THUMBPOSITION: {
        SCROLLINFO siP = {};
        siP.cbSize = sizeof(siP);
        siP.fMask = SIF_POS;
        if (GetScrollInfo(m_hwnd, SB_HORZ, &siP))
            m_scrollX = (int)siP.nPos;
        else
            m_scrollX = pos;
        break;
    }
    case SB_LEFT:       m_scrollX = 0; break;
    case SB_RIGHT:      m_scrollX = maxScrollX(); break;
    }
    m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ViewerWin32::updateScrollBars() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int vw = (std::max)(1, static_cast<int>(rc.right) - sidebarLeft());
    const int vh = (std::max)(1, static_cast<int>(rc.bottom) - pageAreaTop());

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    if (!m_controller || !m_controller->hasDocument() || m_controller->isPagedMode()) {
        if (m_controller && m_controller->hasDocument() && m_controller->isPagedMode()) {
            // Paged mode: if the current page overflows the viewport width,
            // enable the horizontal scrollbar. nMax is the page width minus 1,
            // nPage the viewport width, so the reachable track is
            // nMax - nPage + 1 = pageW - viewportW (the overflow amount).
            const QRect pr = m_controller->pageRect(m_controller->currentPage());
            const int pageW = pr.isValid() ? pr.width() : 0;
            if (pageW > vw && pageW > 0) {
                si.nMin = 0;
                si.nMax = pageW - 1;
                si.nPage = vw;
                si.nPos = m_scrollX;
                SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
            } else {
                si.nMin = 0; si.nMax = 0; si.nPage = 1; si.nPos = 0;
                SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
            }
        } else {
            si.nMin = 0; si.nMax = 0; si.nPage = 1; si.nPos = 0;
            SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
        }

        si.nMin = 0;
        si.nMax = (std::max)(0, m_controller ? m_controller->pageCount() - 1 : 0);
        si.nPage = 1;
        si.nPos = m_controller ? m_controller->currentPage() - 1 : 0;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
        return;
    }

    const QSize cs = m_controller->contentSize();

    si.nMin = 0;
    si.nMax = (std::max)(0, cs.width() - 1);
    si.nPage = vw;
    si.nPos = m_scrollX;
    SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);

    si.nMin = 0;
    si.nMax = (std::max)(0, cs.height() - 1);
    si.nPage = vh;
    si.nPos = m_scrollY;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

void ViewerWin32::updateVisiblePage() {
    if (!m_controller || m_controller->isPagedMode())
        return;
    const int page = m_controller->pageAtScrollOffset(m_scrollY);
    if (page != m_controller->currentPage()) {
        m_controller->trackCurrentPage(page);
        m_controller->setScrollAnchor(m_scrollY);
        m_toolbarPresenter.refreshState();
        m_sidebarPresenter.onPageChanged(page);
    }
}

#endif // Q_OS_WIN
