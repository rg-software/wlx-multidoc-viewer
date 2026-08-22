#include "viewer_win32.h"
#include "viewer_settings.h"

#ifdef Q_OS_WIN

#include <algorithm>

#include <QImage>
#include <QVector>

#include <windowsx.h>

#define WLX_VIEWER_CLASS L"WLXDocViewer"
#define WLX_INFO_CLASS L"WLXDocInfoPanel"

namespace {
constexpr COLORREF kBgColor = 0x808080;
constexpr COLORREF kPanelBg = 0x202020;
constexpr COLORREF kPanelFg = 0xFFFFFF;
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
    ViewerController* m_controller = nullptr;
};

InfoPanelWin32::InfoPanelWin32(HWND hParent) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WLX_INFO_CLASS;

    static bool registered = false;
    if (!registered) {
        RegisterClassExW(&wc);
        registered = true;
    }

    m_hwnd = CreateWindowExW(
        0, WLX_INFO_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        hParent, nullptr, hInst, this);

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

void InfoPanelWin32::setController(ViewerController* controller) {
    m_controller = controller;
}

int InfoPanelWin32::height() const {
    return ViewerController::kInfoPanelHeight;
}

void InfoPanelWin32::onControllerChanged() {
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void InfoPanelWin32::onSize(int w, int h) {
    Q_UNUSED(h)
    MoveWindow(m_hwnd, 0, 0, w, height(), TRUE);
}

LRESULT CALLBACK InfoPanelWin32::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<InfoPanelWin32*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self)
        return self->handleMsg(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT InfoPanelWin32::handleMsg(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        onPaint();
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

void InfoPanelWin32::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HBRUSH bg = CreateSolidBrush(kPanelBg);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    if (m_controller && m_controller->hasDocument()) {
        const bool continuous = !m_controller->isPagedMode();
        QString mode = continuous ? QStringLiteral("Continuous: ON") : QStringLiteral("Continuous: OFF");
        QString fitMode;
        switch (m_controller->fitMode()) {
        case ViewerController::FitMode::FitToPage:  fitMode = QStringLiteral("Fit: Page"); break;
        case ViewerController::FitMode::FitToWidth: fitMode = QStringLiteral("Fit: Width"); break;
        case ViewerController::FitMode::Manual: {
            int pct = static_cast<int>(m_controller->zoom() * 100 + 0.5);
            fitMode = QStringLiteral("Zoom: %1%").arg(pct);
            break;
        }
        }
        QString text = QStringLiteral("%1 / %2   |   %3   |   %4")
                           .arg(m_controller->currentPage())
                           .arg(m_controller->pageCount())
                           .arg(mode)
                           .arg(fitMode);
        SetTextColor(hdc, kPanelFg);
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, reinterpret_cast<LPCWSTR>(text.utf16()), -1, &rc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    EndPaint(m_hwnd, &ps);
}

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
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL | WS_HSCROLL,
        0, 0, 0, 0,
        hParent, nullptr, hInst, this);

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    m_infoPanel = std::make_unique<InfoPanelWin32>(m_hwnd);
    m_controller = std::make_unique<ViewerController>();
    m_controller->setStateChangedCallback([this]() { onControllerChanged(); });
    m_controller->setDpiScale(static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi);
    m_infoPanel->setController(m_controller.get());
}

ViewerWin32::~ViewerWin32() {
    closeDocument();
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
    onControllerChanged();
    return true;
}

void ViewerWin32::closeDocument() {
    invalidatePageBitmaps();
    if (m_controller)
        m_controller->closeDocument();
    m_scrollX = 0;
    m_scrollY = 0;
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void ViewerWin32::onControllerChanged() {
    if (!m_controller)
        return;
    m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
    m_scrollY = (std::clamp)(m_scrollY, 0, maxScrollY());
    updateScrollBars();
    if (m_infoPanel)
        m_infoPanel->onControllerChanged();
    InvalidateRect(m_hwnd, nullptr, FALSE);
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
    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: // CS_DBLCLKS class: a fast second press must still drag
        onDragStart(lp);
        if (!m_dragging)
            break;
        return 0;
    case WM_MOUSEMOVE:
        if (!m_dragging)
            break;
        onDragMove(lp);
        return 0;
    case WM_LBUTTONUP:
        if (!m_dragging)
            break;
        onDragEnd();
        return 0;
    case WM_SETCURSOR:
        if (m_dragging && LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
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

    const int panelH = m_infoPanel ? m_infoPanel->height() : 0;

    const bool paged = !m_controller || m_controller->isPagedMode();
    if (paged) {
        if (m_controller && m_controller->hasDocument()) {
            const int vh = h - panelH;
            if (vh > 0) {
                QRect r = m_controller->pageRect(m_controller->currentPage());
                int imgW = r.isValid() ? r.width() : 0;
                int imgH = r.isValid() ? r.height() : 0;
                if (imgW > 0 && imgH > 0) {
                    // Center the page only when it fits; otherwise scroll the
                    // visible part with m_scrollX/m_scrollY offset.
                    int dstX = (imgW <= w) ? (std::max)(0, (w - imgW) / 2) : -m_scrollX;
                    int dstY = panelH + ((imgH <= vh) ? (std::max)(0, (vh - imgH) / 2) : -m_scrollY);
                    drawPageBitmap(hdcMem, bitmapForPage(m_controller->currentPage()),
                                   dstX, dstY, 0, 0, imgW, imgH);
                }
            }
        }
    } else {
        const int vh = h - panelH;
        const QSize cs = m_controller->contentSize();
        const int cx = (std::max)(0, (w - cs.width()) / 2);
        const int cy = (std::max)(0, (vh - cs.height()) / 2);

        const int firstVisible = m_controller->firstPageAtScroll(m_scrollY);
        for (int page = firstVisible; page <= m_controller->pageCount(); ++page) {
            const QRect r = m_controller->pageRect(page);
            if (r.bottom() < m_scrollY)
                continue;
            if (r.top() > m_scrollY + vh)
                break;
            const int dstX = cx + r.x() - m_scrollX;
            const int dstY = panelH + cy + (r.y() - m_scrollY);
            if (dstX >= w || dstY >= h)
                continue;
            drawPageBitmap(hdcMem, bitmapForPage(page), dstX, dstY, 0, 0, r.width(), r.height());
        }
        m_controller->trimRenderCache(m_scrollY);
    }

    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

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
    if (m_infoPanel)
        m_infoPanel->onSize(w, h);
    if (m_controller) {
        if (m_hwnd)
            m_controller->setDpiScale(static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi);
        m_controller->setViewportSize(QSize(w, h));
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
    if (!m_controller || !m_controller->hasDocument())
        return;

    bool captured = false;
    bool continuous = !m_controller->isPagedMode();

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
        // captured at the top of onKeyDown (paged<->continuous). Refresh it so
        // the post-switch scroll positioning below uses the NEW mode; otherwise
        // switching paged->continuous restarts at page 1.
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
    const int panelH = m_infoPanel ? m_infoPanel->height() : 0;
    const int vw = (std::max)(1, static_cast<int>(rc.right));
    const int vh = (std::max)(1, static_cast<int>(rc.bottom) - panelH);

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
    int page = m_controller->pageAtScrollOffset(m_scrollY);
    if (page != m_controller->currentPage()) {
        m_controller->trackCurrentPage(page);
        if (m_infoPanel)
            m_infoPanel->onControllerChanged();
    }
}

#endif // Q_OS_WIN