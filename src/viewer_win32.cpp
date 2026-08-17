#include "viewer_win32.h"

#ifdef Q_OS_WIN

#include <QFileInfo>

#define WLX_VIEWER_CLASS L"WLXDocViewer"

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
}

ViewerWin32::~ViewerWin32() {
    closeDocument();
    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

bool ViewerWin32::loadDocument(const QString& path) {
    closeDocument();

    m_engine = createEngine(path);
    if (!m_engine || !m_engine->open(path))
        return false;

    m_currentPage = 1;
    m_zoom = 1.0f;
    m_fitToWidth = true;

    renderCurrentPage();
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, TRUE);
    return true;
}

void ViewerWin32::closeDocument() {
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }
    m_currentImage = QImage();
    if (m_engine) {
        m_engine->close();
        m_engine.reset();
    }
    m_currentPage = 1;
    m_scrollX = 0;
    m_scrollY = 0;
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
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        m_scrollY = qMax(0, m_scrollY - delta);
        updateScrollBars();
        InvalidateRect(m_hwnd, nullptr, TRUE);
        return 0;
    }
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

    if (m_hBitmap) {
        HDC hdcMem = CreateCompatibleDC(hdc);
        HGDIOBJ hOld = SelectObject(hdcMem, m_hBitmap);

        int imgW = m_currentImage.width();
        int imgH = m_currentImage.height();

        int srcX = m_scrollX;
        int srcY = m_scrollY;
        int dstW = qMin(imgW - srcX, rc.right);
        int dstH = qMin(imgH - srcY, rc.bottom);

        if (dstW > 0 && dstH > 0)
            BitBlt(hdc, 0, 0, dstW, dstH, hdcMem, srcX, srcY, SRCCOPY);

        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
    } else {
        FillRect(hdc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    }

    EndPaint(m_hwnd, &ps);
}

void ViewerWin32::onSize(int w, int h) {
    Q_UNUSED(w)
    Q_UNUSED(h)

    if (m_fitToWidth && m_engine && m_engine->isOpen()) {
        PageInfo info = m_engine->pageDimensions(m_currentPage);
        if (info.width > 0) {
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            m_zoom = static_cast<float>(rc.right) / info.width;
        }
        renderCurrentPage();
    }

    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void ViewerWin32::onVScroll(int code, int pos) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(m_hwnd, SB_VERT, &si);

    switch (code) {
    case SB_LINEUP:    m_scrollY -= 20; break;
    case SB_LINEDOWN:  m_scrollY += 20; break;
    case SB_PAGEUP:    m_scrollY -= si.nPage; break;
    case SB_PAGEDOWN:  m_scrollY += si.nPage; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        m_scrollY = pos;
        break;
    }

    m_scrollY = qBound(0, m_scrollY, qMax(0, si.nMax - static_cast<int>(si.nPage)));
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, TRUE);
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
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        m_scrollX = pos;
        break;
    }

    m_scrollX = qBound(0, m_scrollX, qMax(0, si.nMax - static_cast<int>(si.nPage)));
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void ViewerWin32::updateScrollBars() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    int imgW = m_currentImage.width();
    int imgH = m_currentImage.height();

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    si.nMin = 0;
    si.nMax = imgW;
    si.nPage = rc.right;
    si.nPos = m_scrollX;
    SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);

    si.nMax = imgH;
    si.nPage = rc.bottom;
    si.nPos = m_scrollY;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

void ViewerWin32::renderCurrentPage() {
    if (!m_engine || !m_engine->isOpen())
        return;

    m_currentImage = m_engine->renderPage(m_currentPage, m_zoom);
    imageToBitmap();
}

void ViewerWin32::imageToBitmap() {
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }

    if (m_currentImage.isNull())
        return;

    QImage img = m_currentImage.convertToFormat(QImage::Format_RGB888);
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

    int stride = w * 3;
    int srcStride = img.bytesPerLine();
    auto* dst = static_cast<uchar*>(bits);
    const uchar* src = img.constBits();

    for (int y = 0; y < h; ++y) {
        memcpy(dst + y * stride, src + y * srcStride, stride);
    }
}

#endif // Q_OS_WIN
