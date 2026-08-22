#include "viewer_win32.h"

#ifdef Q_OS_WIN

#include <algorithm>

#include <QImage>

#include <QFileInfo>

#define WLX_VIEWER_CLASS L"WLXDocViewer"
#define WLX_INFO_CLASS L"WLXDocInfoPanel"

namespace {
constexpr int kPageMargin = 8;
constexpr COLORREF kBgColor = 0x808080;
constexpr COLORREF kPanelBg = 0x202020;
constexpr COLORREF kPanelFg = 0xFFFFFF;
}

InfoPanelWin32::InfoPanelWin32(HWND hParent) : m_hParent(hParent) {
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
    return 22;
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
        QString mode = m_controller->isPagedMode() ? QStringLiteral("Continuous: OFF") : QStringLiteral("Continuous: ON");
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

ViewerWin32::ViewerWin32(HWND hParent)
    : m_hParent(hParent)
{
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
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }
    m_currentImage = QImage();
    if (m_controller)
        m_controller->closeDocument();
    m_scrollX = 0;
    m_scrollY = 0;
}

void ViewerWin32::onControllerChanged() {
    m_currentImage = m_controller->renderVisiblePages(m_scrollY);
    imageToBitmap(m_currentImage);
    m_renderedScrollY = m_scrollY;
    m_renderedPageCount = m_controller->pageCount();
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
    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

void ViewerWin32::onKeyDown(WPARAM wp, bool shift) {
    if (!m_controller || !m_controller->hasDocument())
        return;

    int captured = 0;
    const bool continuous = !m_controller->isPagedMode();

    switch (wp) {
    case VK_RIGHT:
    case VK_NEXT:
        if (continuous) {
            m_scrollY += m_controller->pageStride();
        } else {
            m_controller->nextPage();
        }
        captured = 1;
        break;
    case VK_LEFT:
    case VK_PRIOR:
        if (continuous) {
            m_scrollY -= m_controller->pageStride();
        } else {
            m_controller->prevPage();
        }
        captured = 1;
        break;
    case VK_UP:
        if (continuous)
            m_scrollY -= 60;
        else
            m_controller->prevPage();
        captured = 1;
        break;
    case VK_DOWN:
        if (continuous)
            m_scrollY += 60;
        else
            m_controller->nextPage();
        captured = 1;
        break;
    case VK_HOME:
        if (continuous) {
            m_scrollY = 0;
        } else {
            m_controller->firstPage();
        }
        captured = 1;
        break;
    case VK_END:
        if (continuous) {
            m_scrollY = (std::max)(0, m_currentImage.height() - 1);
        } else {
            m_controller->lastPage();
        }
        captured = 1;
        break;
    case 'V':
        if (shift)
            m_controller->cycleFitMode();
        else
            m_controller->toggleMode();
        m_scrollX = 0;
        m_scrollY = (m_controller->currentPage() - 1) * m_controller->pageStride();
        captured = 1;
        break;
    case 'R':
        if (shift)
            m_controller->rotateCcw();
        else
            m_controller->rotateCw();
        captured = 1;
        break;
    case 0xBB: case 0x6B:
        m_controller->zoomIn();
        captured = 1;
        break;
    case 0xBD: case 0x6D:
        m_controller->zoomOut();
        captured = 1;
        break;
    case '0':
        m_controller->setManualZoom(1.0f);
        captured = 1;
        break;
    }

    if (captured) {
        if (continuous) {
            int maxY = (std::max)(0, m_currentImage.height() - 1);
            m_scrollY = (std::clamp)(m_scrollY, 0, maxY);
            updateVisiblePage();
            if (needsStripRerender())
                onControllerChanged();
            else {
                updateScrollBars();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        } else if (wp == VK_RIGHT || wp == VK_LEFT || wp == VK_NEXT || wp == VK_PRIOR
                   || wp == VK_HOME || wp == VK_END || wp == 'V') {
            m_scrollX = 0;
            m_scrollY = 0;
        }
    }
}

void ViewerWin32::onMouseWheel(int delta) {
    if (!m_controller)
        return;
    if (m_controller->isPagedMode()) {
        if (delta < 0)
            m_controller->nextPage();
        else
            m_controller->prevPage();
        m_scrollX = 0;
        m_scrollY = 0;
    } else {
        const int step = 60;
        m_scrollY = (std::max)(0, m_scrollY - delta * step / WHEEL_DELTA);
        int maxY = (std::max)(0, m_currentImage.height() - 1);
        m_scrollY = (std::clamp)(m_scrollY, 0, maxY);
        updateVisiblePage();
        if (needsStripRerender())
            onControllerChanged();
        else {
            updateScrollBars();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
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
    const int pageY = panelH;

    if (m_hBitmap) {
        HDC hdcStrip = CreateCompatibleDC(hdc);
        HGDIOBJ hOld = SelectObject(hdcStrip, m_hBitmap);

        int imgW = m_currentImage.width();
        int imgH = m_currentImage.height();

        int srcX = m_scrollX;
        int srcY = m_scrollY;
        int availW = w - 2 * kPageMargin;
        int availH = h - pageY - 2 * kPageMargin;
        int dstW = (std::min)(imgW - srcX, availW);
        int dstH = (std::min)(imgH - srcY, availH);

        int dstX = kPageMargin;
        int dstY = pageY + kPageMargin;
        if (imgW < availW)
            dstX += (availW - imgW) / 2;
        if (imgH < availH)
            dstY += (availH - imgH) / 2;

        if (dstW > 0 && dstH > 0)
            BitBlt(hdcMem, dstX, dstY, dstW, dstH, hdcStrip, srcX, srcY, SRCCOPY);

        SelectObject(hdcStrip, hOld);
        DeleteDC(hdcStrip);
    }

    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);

    EndPaint(m_hwnd, &ps);
}

void ViewerWin32::onSize(int w, int h) {
    if (m_infoPanel)
        m_infoPanel->onSize(w, h);
    if (m_controller) {
        m_controller->setViewportSize(QSize(w, h));
        onControllerChanged();
    }
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
        }
    } else {
        switch (code) {
        case SB_LINEUP:    m_scrollY -= 20; break;
        case SB_LINEDOWN:  m_scrollY += 20; break;
        case SB_PAGEUP:    m_scrollY -= si.nPage; break;
        case SB_PAGEDOWN:  m_scrollY += si.nPage; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION:
            m_scrollY = pos; break;
        case SB_TOP:       m_scrollY = 0; break;
        case SB_BOTTOM:    m_scrollY = si.nMax; break;
        }
        int maxY = (std::max)(0, static_cast<int>(si.nMax) - static_cast<int>(si.nPage));
        m_scrollY = (std::clamp)(m_scrollY, 0, maxY);
        updateVisiblePage();
        updateScrollBars();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ViewerWin32::onHScroll(int code, int pos) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(m_hwnd, SB_HORZ, &si);

    switch (code) {
    case SB_LINELEFT:   m_scrollX -= 20; break;
    case SB_LINERIGHT:  m_scrollX += 20; break;
    case SB_PAGELEFT:   m_scrollX -= si.nPage; break;
    case SB_PAGERIGHT:  m_scrollX += si.nPage; break;
    case SB_THUMBTRACK: case SB_THUMBPOSITION:
        m_scrollX = pos; break;
    case SB_LEFT:       m_scrollX = 0; break;
    case SB_RIGHT:      m_scrollX = si.nMax; break;
    }
    int maxX = (std::max)(0, static_cast<int>(si.nMax) - static_cast<int>(si.nPage));
    m_scrollX = (std::clamp)(m_scrollX, 0, maxX);

    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ViewerWin32::updateScrollBars() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int panelH = m_infoPanel ? m_infoPanel->height() : 0;
    const int vw = (std::max)(1, static_cast<int>(rc.right) - 2 * kPageMargin);
    const int vh = (std::max)(1, static_cast<int>(rc.bottom) - panelH - 2 * kPageMargin);

    int imgW = m_currentImage.width();
    int imgH = m_currentImage.height();

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    if (!m_controller || m_controller->isPagedMode()) {
        si.nMin = 0;
        si.nMax = (std::max)(0, m_controller ? m_controller->pageCount() - 1 : 0);
        si.nPage = 1;
        si.nPos = m_controller ? m_controller->currentPage() - 1 : 0;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

        si.nMin = 0; si.nMax = 0; si.nPage = 1; si.nPos = 0;
        SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
    } else {
        si.nMin = 0;
        si.nMax = (std::max)(0, imgW - vw);
        si.nPage = vw;
        si.nPos = m_scrollX;
        SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);

        si.nMin = 0;
        si.nMax = (std::max)(0, imgH - vh);
        si.nPage = vh;
        si.nPos = m_scrollY;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
    }
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

bool ViewerWin32::needsStripRerender() const {
    if (!m_controller || !m_controller->hasDocument())
        return false;
    if (m_renderedPageCount != m_controller->pageCount())
        return true;
    int vh = m_controller->pageAreaHeight();
    int dist = abs(m_scrollY - m_renderedScrollY);
    return dist > vh;
}

void ViewerWin32::imageToBitmap(const QImage& src) {
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }
    if (src.isNull())
        return;

    QImage img = src.convertToFormat(QImage::Format_RGB888);
    int w = img.width();
    int h = img.height();

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    void* bits = nullptr;
    m_hBitmap = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bi),
                                  DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!m_hBitmap || !bits)
        return;

    int dibStride = ((w * 3 + 3) / 4) * 4;
    int srcStride = img.bytesPerLine();
    auto* dst = static_cast<uchar*>(bits);
    const uchar* srcd = img.constBits();

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
}

void ViewerWin32::ensureInfoPanel() {
    if (!m_infoPanel) {
        m_infoPanel = std::make_unique<InfoPanelWin32>(m_hwnd);
        m_infoPanel->setController(m_controller.get());
    }
}

#endif // Q_OS_WIN
