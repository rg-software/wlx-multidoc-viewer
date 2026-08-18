#include "viewer_win32.h"

#ifdef Q_OS_WIN

#include <QFileInfo>

#define WLX_VIEWER_CLASS L"WLXDocViewer"

static const int kPageMargin = 8;
static const COLORREF kBgColor = 0x808080;

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

    m_state.setPageCount(m_engine->pageCount());
    m_state.resetPage();
    m_state.resetZoom();

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
    m_state = ViewerState();
    m_scrollX = 0;
    m_scrollY = 0;
}

float ViewerWin32::dpiScale() const {
    UINT dpi = 96;
    if (auto* user32 = GetModuleHandleW(L"user32.dll")) {
        using GetDpiForWindow_t = UINT(WINAPI*)(HWND);
        auto pGetDpi = reinterpret_cast<GetDpiForWindow_t>(
            GetProcAddress(user32, "GetDpiForWindow"));
        if (pGetDpi)
            dpi = pGetDpi(m_hwnd);
    }
    return static_cast<float>(dpi) / 96.0f;
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
        onKeyDown(wp);
        return 0;
    case WM_CHAR:
        onChar(wp);
        return 0;
    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        HBRUSH bgBrush = CreateSolidBrush(kBgColor);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);
        return 1;
    }
    }

    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

void ViewerWin32::onKeyDown(WPARAM wp) {
    bool needRender = false;
    bool needNav = false;

    switch (wp) {
    case VK_RIGHT:
    case VK_NEXT:
        needNav = m_state.nextPage();
        break;
    case VK_LEFT:
    case VK_PRIOR:
        needNav = m_state.prevPage();
        break;
    case VK_HOME:
        needNav = m_state.firstPage();
        break;
    case VK_END:
        needNav = m_state.lastPage();
        break;
    case 'V':
        m_state.setPagedMode(!m_state.isPagedMode());
        m_scrollX = 0;
        m_scrollY = 0;
        if (!m_state.isPagedMode() && m_state.autoFit())
            applyFitZoom();
        renderCurrentPage();
        updateScrollBars();
        InvalidateRect(m_hwnd, nullptr, TRUE);
        return;
    case 0xBB: // '+' (OEM_PLUS on US layout)
    case 0x6B: // numpad '+'
        m_state.zoomIn();
        needRender = true;
        break;
    case 0xBD: // '-' (OEM_MINUS on US layout)
    case 0x6D: // numpad '-'
        m_state.zoomOut();
        needRender = true;
        break;
    case '0':
        m_state.setManualZoom(1.0f);
        needRender = true;
        break;
    case 'W':
        m_state.setFitToWidth();
        applyFitZoom();
        needRender = true;
        break;
    case 'P':
        m_state.setFitToPage();
        applyFitZoom();
        needRender = true;
        break;
    }

    if (needNav) {
        m_scrollX = 0;
        m_scrollY = 0;
        needRender = true;
    }

    if (needRender) {
        renderCurrentPage();
        updateScrollBars();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void ViewerWin32::onChar(WPARAM wp) {
    if (wp == '0') {
        m_state.setManualZoom(1.0f);
        renderCurrentPage();
        updateScrollBars();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void ViewerWin32::onMouseWheel(int delta) {
    if (m_state.isPagedMode()) {
        if (delta < 0)
            m_state.nextPage();
        else
            m_state.prevPage();
        m_scrollX = 0;
        m_scrollY = 0;
        renderCurrentPage();
        updateScrollBars();
    } else {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int vh = rc.bottom - 2 * kPageMargin;
        int imgH = m_currentImage.height();
        int maxScroll = qMax(0, imgH - vh);
        m_scrollY = qBound(0, m_scrollY - delta * 3, maxScroll);
        updateScrollBars();
    }
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void ViewerWin32::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);

    HBRUSH bgBrush = CreateSolidBrush(kBgColor);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    if (m_hBitmap) {
        HDC hdcMem = CreateCompatibleDC(hdc);
        HGDIOBJ hOld = SelectObject(hdcMem, m_hBitmap);

        int imgW = m_currentImage.width();
        int imgH = m_currentImage.height();

        int srcX = m_scrollX;
        int srcY = m_scrollY;
        int dstW = qMin(imgW - srcX, rc.right);
        int dstH = qMin(imgH - srcY, rc.bottom);

        int dstX = kPageMargin;
        int dstY = kPageMargin;

        if (dstW > 0 && dstH > 0)
            BitBlt(hdc, dstX, dstY, dstW, dstH, hdcMem, srcX, srcY, SRCCOPY);

        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
    }

    if (m_state.pageCount() > 0) {
        wchar_t buf[64];
        const wchar_t* mode = m_state.isPagedMode() ? L"Page" : L"Cont";
        swprintf_s(buf, L"%d/%d  [%s]", m_state.currentPage(), m_state.pageCount(), mode);
        SetTextColor(hdc, 0xFFFFFF);
        SetBkMode(hdc, TRANSPARENT);
        RECT textRc = { rc.left, rc.top, rc.right - GetSystemMetrics(SM_CXVSCROLL), rc.bottom - GetSystemMetrics(SM_CYHSCROLL) };
        DrawTextW(hdc, buf, -1, &textRc, DT_BOTTOM | DT_RIGHT | DT_NOCLIP);
    }

    EndPaint(m_hwnd, &ps);
}

void ViewerWin32::applyFitZoom() {
    if (!m_engine || !m_engine->isOpen())
        return;

    if (!m_state.autoFit())
        return;

    PageInfo info = m_engine->pageDimensions(m_state.currentPage());
    if (info.width <= 0)
        return;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int vw = qMax(1, rc.right - 2 * kPageMargin);
    int vh = qMax(1, rc.bottom - 2 * kPageMargin);

    int sbWidth = GetSystemMetrics(SM_CXVSCROLL);
    int sbHeight = GetSystemMetrics(SM_CYHSCROLL);

    float ds = dpiScale();

    if (m_state.fitToWidth()) {
        m_state.setZoom(m_state.fitToWidthZoom(info.width, qMax(1, vw - sbWidth)) / ds);
    } else {
        m_state.setZoom(m_state.fitToPageZoom(info.width, info.height,
                        qMax(1, vw - sbWidth), qMax(1, vh - sbHeight)) / ds);
    }
}

void ViewerWin32::onSize(int w, int h) {
    Q_UNUSED(w)
    Q_UNUSED(h)

    if (m_engine && m_engine->isOpen()) {
        applyFitZoom();
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

    if (m_state.isPagedMode()) {
        bool changed = false;
        switch (code) {
        case SB_LINEDOWN:
        case SB_PAGEDOWN:
            changed = m_state.nextPage();
            break;
        case SB_LINEUP:
        case SB_PAGEUP:
            changed = m_state.prevPage();
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            changed = m_state.goToPage(pos + 1);
            break;
        case SB_TOP:
            changed = m_state.firstPage();
            break;
        case SB_BOTTOM:
            changed = m_state.lastPage();
            break;
        }
        if (changed) {
            m_scrollX = 0;
            m_scrollY = 0;
            renderCurrentPage();
        }
    } else {
        switch (code) {
        case SB_LINEUP:    m_scrollY -= 20; break;
        case SB_LINEDOWN:  m_scrollY += 20; break;
        case SB_PAGEUP:    m_scrollY -= si.nPage; break;
        case SB_PAGEDOWN:  m_scrollY += si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            m_scrollY = pos;
            break;
        case SB_TOP:       m_scrollY = 0; break;
        case SB_BOTTOM:    m_scrollY = si.nMax; break;
        }
        m_scrollY = qBound(0, m_scrollY, qMax(0, si.nMax - static_cast<int>(si.nPage)));
    }

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
    case SB_LEFT:       m_scrollX = 0; break;
    case SB_RIGHT:      m_scrollX = si.nMax; break;
    }

    m_scrollX = qBound(0, m_scrollX, qMax(0, si.nMax - static_cast<int>(si.nPage)));
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void ViewerWin32::updateScrollBars() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    int vw = rc.right - 2 * kPageMargin;
    int vh = rc.bottom - 2 * kPageMargin;

    int imgW = m_currentImage.width();
    int imgH = m_currentImage.height();

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    if (m_state.isPagedMode()) {
        si.nMin = 0;
        si.nMax = m_state.pageCount() - 1;
        si.nPage = 1;
        si.nPos = m_state.currentPage() - 1;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

        si.nMin = 0;
        si.nMax = 0;
        si.nPage = 1;
        si.nPos = 0;
        SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
    } else {
        si.nMin = 0;
        si.nMax = qMax(0, imgW - vw);
        si.nPage = vw;
        si.nPos = m_scrollX;
        SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);

        si.nMin = 0;
        si.nMax = qMax(0, imgH - vh);
        si.nPage = vh;
        si.nPos = m_scrollY;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
    }
}

void ViewerWin32::renderCurrentPage() {
    if (!m_engine || !m_engine->isOpen())
        return;

    m_currentImage = m_engine->renderPage(m_state.currentPage(), m_state.zoom(), dpiScale());
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

    int dibStride = ((w * 3 + 3) / 4) * 4;
    int srcStride = img.bytesPerLine();
    auto* dst = static_cast<uchar*>(bits);
    const uchar* src = img.constBits();

    for (int y = 0; y < h; ++y) {
        memcpy(dst + y * dibStride, src + y * srcStride, w * 3);
    }
}

#endif // Q_OS_WIN
