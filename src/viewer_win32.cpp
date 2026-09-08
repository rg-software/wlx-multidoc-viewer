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
// SetTimer id/owner for in-place GIF playback. The message-based timer delivers
// WM_TIMER (with the id in wParam), so no custom message is needed.
constexpr UINT kAnimTimerId = 1;

void marshalToWnd(HWND hwnd, std::function<void()> task) {
    if (!hwnd)
        return;
    {
        std::lock_guard<std::mutex> lock(g_marshalMutex);
        g_marshalQueues[hwnd].push_back(std::move(task));
    }
    PostMessageW(hwnd, WM_PLUGIN_MARSHAL, 0, 0);
}

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
        WS_EX_COMPOSITED, WLX_VIEWER_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
        0, 0, 0, 0,
        hParent, nullptr, hInst, this);

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    m_controller = std::make_unique<ViewerController>();
    m_controller->setStateChangedCallback([this]() { onControllerChanged(); });
    m_controller->setLayoutScale(static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi);
    m_controller->setRenderScale(static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi);
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
    m_sidebar->setWidthChangedHandler([this](int logicalPx) {
        // Clamp the drag candidate to the shared bounds: min 80 logical px,
        // max half the page area (= clientLogicalW / 3 while the sidebar is
        // visible, never below the min so a tiny lister degrades gracefully).
        if (!m_controller)
            return;
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        const float dpi = static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi;
        const int clientLogicalW = static_cast<int>(rc.right / dpi);
        const int maxAllowed = (std::max)(viewer_settings::kSidebarMinWidth, clientLogicalW / 3);
        m_sidebar->setBaseWidth((std::clamp)(logicalPx, viewer_settings::kSidebarMinWidth, maxAllowed));
        layoutChrome();
        m_controller->setLeftChrome(sidebarLeft());
        m_scrollY = m_controller->relayout(m_scrollY);
        m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
        m_scrollY = (std::clamp)(m_scrollY, 0, maxScrollY());
        onControllerChanged();
    });
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
    // An outlined document starts with the sidebar shown only when the INI
    // [Viewer] SidebarVisible key is enabled; manual toggling is never overridden.
    const bool startVisible =
        m_sidebarPresenter.hasOutline() && viewer_settings::kSidebarVisibleByDefault;
    showHideSidebar(startVisible);
    m_toolbar->setChecked(toolbar::Control::SidebarToggle, startVisible);
    onControllerChanged();
    // reload() runs after openDocument() (which already fired refreshState),
    // so re-sync the toolbar now that sidebarAvailable()/hasOutline() are real.
    m_toolbarPresenter.refreshState();
    return true;
}

void ViewerWin32::closeDocument() {
    stopAnimationTimer();
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
    syncAnimationTimer();
}

// ---------------------------------------------------------------------------
// In-place GIF playback (tasks 6.1-6.2)
// ---------------------------------------------------------------------------

void ViewerWin32::invalidatePageArea() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    RECT pa = {
        static_cast<LONG>(sidebarLeft()),
        static_cast<LONG>(pageAreaTop()),
        rc.right,
        rc.bottom
    };
    InvalidateRect(m_hwnd, &pa, FALSE);
}

void ViewerWin32::syncAnimationTimer() {
    stopAnimationTimer();
    if (!m_controller || !m_controller->isAnimating())
        return;
    const int delay = (std::max)(10, m_controller->animationDelayMs());
    if (delay > 0)
        SetTimer(m_hwnd, kAnimTimerId, static_cast<UINT>(delay), nullptr);
}

void ViewerWin32::stopAnimationTimer() {
    if (m_hwnd)
        KillTimer(m_hwnd, kAnimTimerId);
}

void ViewerWin32::onAnimationTick() {
    if (!m_controller || !m_controller->animationTick()) {
        stopAnimationTimer();
        return;
    }
    // The controller advanced a frame and invalidated its page cache; repaint
    // just the page area (bitmapForPage re-renders on the animation-epoch bump).
    invalidatePageArea();
    const int delay = (std::max)(10, m_controller->animationDelayMs());
    if (delay > 0)
        SetTimer(m_hwnd, kAnimTimerId, static_cast<UINT>(delay), nullptr);
    else
        stopAnimationTimer();
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

    if (m_toolbar && m_toolbar->hwnd()) {
        // Skip the forced repaint when the toolbar's geometry is unchanged
        // (during a sidebar drag the strip never moves, so MoveWindow(..., TRUE)
        // redraws it on every mouse move for nothing). A real size change is
        // still fully covered by the CS_HREDRAW|CS_VREDRAW class style, which
        // invalidates the whole client on its own. The toolbar spans the full
        // client width above the sidebar, so resizing the panel can never
        // expose or dirty a region under it.
        RECT cur = {};
        const bool sizeChanged =
            GetClientRect(m_toolbar->hwnd(), &cur) &&
            (cur.right - cur.left != w || cur.bottom - cur.top != toolbarHeight());
        MoveWindow(m_toolbar->hwnd(), 0, 0, w, toolbarHeight(), sizeChanged);
    }
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
    // Sync the toolbar's first-page enablement with the *new* anchor. During
    // go-to/step/sidebar jumps the notify from goToPage already refreshed the
    // toolbar against the old scroll (typically page 1 -> Prev disabled, Next
    // enabled); without this refresh the buttons stay stale until a later
    // scroll event, so a Next press on the final unit would silently no-op.
    m_toolbarPresenter.refreshState();
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
    // WM_TIMER carries the timer id in wParam; only the animation timer uses a
    // message-based (null proc) timer, so any WM_TIMER with our id is a frame tick.
    case WM_TIMER:
        if (wp == kAnimTimerId)
            onAnimationTick();
        return 0;
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

    const uint32_t bg = viewer_settings::kBackgroundColor;
    HBRUSH bgBrush = CreateSolidBrush(RGB((bg >> 16) & 0xFF,
                                          (bg >> 8) & 0xFF,
                                          bg & 0xFF));
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
                // Draw every page of the current unit (one page in Single, the
                // spread in Double/DoubleWithCover) at its shared placement.
                const int first = m_controller->unitFirst(m_controller->currentPage());
                const int last = m_controller->unitLast(first);
                for (int page = first; page <= last; ++page) {
                    const QRect on = pagedPageRect(page);
                    if (on.width() <= 0 || on.height() <= 0)
                        continue;
                    drawPageBitmap(hdcMem, bitmapForPage(page),
                                   on.left(), on.top(), 0, 0, on.width(), on.height());
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
    if (m_bitmapEpoch != m_controller->layoutEpoch() ||
        m_bitmapAnimEpoch != m_controller->animationEpoch())
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
    m_bitmapAnimEpoch = m_controller ? m_controller->animationEpoch() : -1;
}

int ViewerWin32::maxScrollX() const {
    if (!m_controller || !m_controller->hasDocument())
        return 0;
    if (m_controller->isPagedMode())
        return m_controller->maxScrollOffsetXForUnit(
            m_controller->unitFirst(m_controller->currentPage()));
    return m_controller->maxScrollOffsetX();
}

int ViewerWin32::maxScrollY() const {
    if (!m_controller || !m_controller->hasDocument())
        return 0;
    if (m_controller->isPagedMode())
        return m_controller->maxScrollOffsetYForUnit(
            m_controller->unitFirst(m_controller->currentPage()));
    return m_controller->maxScrollOffset();
}

QRect ViewerWin32::pagedPageRect(int page) const {
    // Mirrors the paint math: the unit is centered in the viewport when it
    // fits, otherwise the visible part is scrolled with m_scrollX/m_scrollY.
    // Each member page is then placed relative to the unit origin.
    if (!m_controller || !m_controller->hasDocument())
        return {};
    const int first = m_controller->unitFirst(m_controller->currentPage());
    const int last = m_controller->unitLast(first);
    if (page < first || page > last)
        return {};
    const QRect r1 = m_controller->pageRect(first);
    const QRect r = m_controller->pageRect(page);
    if (!r1.isValid() || !r.isValid())
        return {};
    const QRect r2 = m_controller->pageRect(last);
    const bool paired = last != first && r2.isValid();
    const int unitW = paired ? (r2.right() - r1.left() + 1) : r1.width();
    const int unitH = paired ? (std::max)(r1.height(), r2.height()) : r1.height();

    const int top = pageAreaTop();
    const int left = sidebarLeft();
    RECT cr;
    GetClientRect(m_hwnd, &cr);
    const int vw = (std::max)(1, static_cast<int>(cr.right) - left);
    const int vh = (std::max)(1, static_cast<int>(cr.bottom) - top);
    const int ox = left + ((unitW <= vw) ? (std::max)(0, (vw - unitW) / 2) : -m_scrollX);
    const int oy = top + ((unitH <= vh) ? (std::max)(0, (vh - unitH) / 2) : -m_scrollY);
    return QRect(ox + (r.left() - r1.left()), oy + (r.top() - r1.top()),
                 r.width(), r.height());
}

void ViewerWin32::onSize(int w, int h) {
    const float dpi = m_hwnd ? static_cast<float>(GetDpiForWindow(m_hwnd)) / kDefaultDpi : 1.0f;
    if (m_toolbar)
        m_toolbar->setDpiScale(dpi);
    if (m_sidebar)
        m_sidebar->setDpiScale(dpi);
    layoutChrome();
    if (m_controller) {
        m_controller->setLayoutScale(dpi);
        m_controller->setRenderScale(dpi);
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

// PgDn/PgUp scroll by one vertical block (one screenful with a small overlap).
// In paged mode, a page/unit that still has overflow scrolls within it; one
// that is fully visible (or within the overlap band of being so, or already at
// its foot) advances to the next/prev page instead of skipping it.
void ViewerWin32::pageBlockDown() {
    if (!m_controller || !m_controller->hasDocument())
        return;
    m_scrollX = 0;
    if (m_controller->isPagedMode()) {
        const int first = m_controller->unitFirst(m_controller->currentPage());
        const int maxY = m_controller->maxScrollOffsetYForUnit(first);
        if (m_controller->unitRequiresVerticalScroll(first) && m_scrollY < maxY) {
            m_scrollY = (std::min)(m_scrollY + m_controller->pageBlockStep(), maxY);
        } else if (m_controller->nextPage()) {
            m_scrollY = 0;
        }
    } else {
        m_scrollY = (std::min)(m_scrollY + m_controller->pageBlockStep(),
                               m_controller->maxScrollOffset());
    }
}

void ViewerWin32::pageBlockUp() {
    if (!m_controller || !m_controller->hasDocument())
        return;
    m_scrollX = 0;
    if (m_controller->isPagedMode()) {
        if (m_controller->unitRequiresVerticalScroll(
                m_controller->unitFirst(m_controller->currentPage())) && m_scrollY > 0) {
            m_scrollY = (std::max)(m_scrollY - m_controller->pageBlockStep(), 0);
        } else if (m_controller->prevPage()) {
            m_scrollY = 0;
        }
    } else {
        m_scrollY = (std::max)(m_scrollY - m_controller->pageBlockStep(), 0);
    }
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
        if (continuous)
            pageJumpContinuous(+1);
        else
            m_controller->nextPage();
        captured = true;
        break;
    case VK_LEFT:
        if (continuous)
            pageJumpContinuous(-1);
        else
            m_controller->prevPage();
        captured = true;
        break;
    case VK_NEXT:
        pageBlockDown();
        captured = true;
        break;
    case VK_PRIOR:
        pageBlockUp();
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
    case 'P':
        // Plain P cycles the page presentation (single / double / double with
        // cover) without touching the paged/continuous mode. Keep the view on
        // the same unit: paged keeps a clean origin, continuous re-targets the
        // scroll to the unit's new position.
        if (!shift) {
            const int page = m_controller->currentPage();
            m_controller->cyclePagePresentation();
            m_scrollX = 0;
            if (continuous)
                m_scrollY = m_controller->scrollOffsetForPage(page);
            else
                m_scrollY = 0;
            captured = true;
        }
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
    case 0x60:          // VK_NUMPAD0
    case 0xBF:          // VK_OEM_2  (forward slash /)
        m_scrollY = m_controller->setManualZoom(1.0f, m_scrollY);
        captured = true;
        break;
    case 0xDC:          // VK_OEM_5  (backslash \)
        onSidebarToggle();
        captured = true;
        break;
    }

    if (captured) {
        if (continuous || wp == VK_NEXT || wp == VK_PRIOR) {
            // Continuous scroll, and paged PgDn/PgUp that scrolled within the
            // current unit (or advanced the page), both land here with a final
            // scroll offset to apply. updateVisiblePage() is a no-op in paged
            // mode and updateScrollBars() refreshes the page/unit indicators.
            m_scrollY = (std::clamp)(m_scrollY, 0, maxScrollY());
            m_scrollX = (std::clamp)(m_scrollX, 0, maxScrollX());
            // Keep the controller's scroll anchor in sync with keyboard-driven
            // continuous motion (arrows/Home/End/P/V/PgDn/PgUp), so the toolbar
            // Prev/Next enablement and stepping always read the live position.
            m_controller->setScrollAnchor(m_scrollY);
            updateVisiblePage();
            updateScrollBars();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        } else if (wp == VK_RIGHT || wp == VK_LEFT
                   || wp == VK_HOME || wp == VK_END || wp == 'V' || wp == 'R'
                   || wp == 0xBB || wp == 0x6B || wp == 0xBD || wp == 0x6D
                   || wp == 0x60 || wp == 0xBF) {
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
        if (!m_controller)
            return 1;
        // In paged double-page view the pointer can sit on either unit member;
        // fall back to the current (unit-first) page for margins/gaps.
        const int first = m_controller->unitFirst(m_controller->currentPage());
        const int last = m_controller->unitLast(first);
        for (int page = first; page <= last; ++page) {
            if (pagedPageRect(page).contains(x, y))
                return page;
        }
        return first;
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
        // The mapping stays in absolute canvas space (pageRect coordinates):
        // the on-screen origin of the member page corresponds to its
        // pageRect.topLeft(), so add that back after removing the screen offset.
        const int first = m_controller->unitFirst(m_controller->currentPage());
        const int last = m_controller->unitLast(first);
        int page = first;
        for (int p = first; p <= last; ++p) {
            if (pagedPageRect(p).contains(x, y)) {
                page = p;
                break;
            }
        }
        const QRect pr = m_controller->pageRect(page);
        const QRect on = pagedPageRect(page);
        if (!pr.isValid() || on.isNull())
            return QPointF(x, y);
        return QPointF(x - on.left() + pr.x(), y - on.top() + pr.y());
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
    const int firstPage = paged ? m_controller->unitFirst(m_controller->currentPage()) : 1;
    const int lastPage = paged ? m_controller->unitLast(firstPage) : m_controller->pageCount();

    // Rects are canvas-space; convert to client by mirroring onPaint placement
    // (canvas pageRect -> on-screen), then draw into the overlay surface.
    auto pageOrigin = [&](int page, const QRect& pr) -> QPointF {
        if (paged) {
            // Shared with onPaint so the highlight always sits exactly on the
            // page bitmap.
            const QRect on = pagedPageRect(page);
            return QPointF(on.left(), on.top());
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

    for (int page = firstPage; page <= lastPage; ++page) {
        const QRect pr = m_controller->pageRect(page);
        if (!pr.isValid())
            continue;
        const QPointF origin = pageOrigin(page, pr);

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
            // Paged mode: if the current page unit overflows the viewport width,
            // enable the horizontal scrollbar. nMax is the unit width minus 1,
            // nPage the viewport width, so the reachable track is
            // nMax - nPage + 1 = unitW - viewportW (the overflow amount). A
            // double-page unit is panned as one group.
            const int first = m_controller->unitFirst(m_controller->currentPage());
            const int last = m_controller->unitLast(first);
            const QRect pr = m_controller->pageRect(first);
            int pageW = pr.isValid() ? pr.width() : 0;
            if (last != first) {
                const QRect pr2 = m_controller->pageRect(last);
                if (pr.isValid() && pr2.isValid())
                    pageW = pr2.right() - pr.left() + 1;
            }
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
