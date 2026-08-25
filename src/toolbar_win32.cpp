#include "toolbar_win32.h"
#include "toolbar_icons.h"

#ifdef Q_OS_WIN

#include <commctrl.h>
#include <windowsx.h>

#include <cstdarg>
#include <cstdio>

#include <QImage>

// No-op debug stub (removed instrumentation). Kept as a defined symbol so any
// residual call sites compile; there should be none in release.
inline void tbLog(const char*, ...) {}

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "comctl32.lib")

#define WLX_TOOLBAR_CLASS L"WLXDocToolbar"

namespace {
enum : int {
    ID_PREV = 1, ID_NEXT,
    ID_PAGE_EDIT, ID_PAGE_STATIC,
    ID_MODE, ID_FIT,
    ID_ROT_L, ID_ROT_R, ID_ZOOM_IN, ID_ZOOM_OUT,
    ID_FIND_PREV, ID_FIND_EDIT, ID_FIND_NEXT, ID_MATCH_CASE, ID_FIND_STATIC,
    ID_PRINT, ID_COPY, ID_SIDEBAR,
};

// Read an edit/static control's current UTF-16 text.
QString textOfEdit(HWND h) {
    if (!h)
        return {};
    const int len = GetWindowTextLengthW(h);
    if (len <= 0)
        return QString();
    QVector<wchar_t> buf(len + 1);
    if (buf.isEmpty())
        return QString();
    GetWindowTextW(h, buf.data(), len + 1);
    return QString::fromWCharArray(buf.data() ? buf.data() : nullptr, len);
}

// QImage -> 32bpp premultiplied-ARGB top-down DIB, for AlphaBlend icon paint.
HBITMAP imageToIconBitmap(const QImage& src, bool grey) {
    if (src.isNull())
        return nullptr;
    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (grey) {
        for (int y = 0; y < img.height(); ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                const int r = qRed(line[x]);
                const int g = qGreen(line[x]);
                const int b = qBlue(line[x]);
                const int gray = (r + g + b) / 3;
                // Distinct but readable disabled state: ~50% alpha, grey tones.
                const int a = qAlpha(line[x]) * 5 / 10;
                line[x] = qRgba(gray, gray, gray, a);
            }
        }
    }
    const int w = img.width();
    const int h = img.height();

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm || !bits)
        return hbm;

    const int stride = ((w * 4 + 3) / 4) * 4;
    for (int y = 0; y < h; ++y) {
        const uchar* srcRow = img.constScanLine(y);
        auto* dst = static_cast<uchar*>(bits) + y * stride;
        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = srcRow[x * 4 + 2]; // B
            dst[x * 4 + 1] = srcRow[x * 4 + 1]; // G
            dst[x * 4 + 2] = srcRow[x * 4 + 0]; // R
            dst[x * 4 + 3] = srcRow[x * 4 + 3]; // A
        }
    }
    return hbm;
}

} // namespace

ToolbarWin32::ToolbarWin32(HWND hParent)
    : m_parent(hParent)
{
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = WLX_TOOLBAR_CLASS;

    static bool registered = false;
    if (!registered) {
        RegisterClassExW(&wc);
        registered = true;
    }

    m_hwnd = CreateWindowExW(
        0, WLX_TOOLBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0, hParent, nullptr, hInst, this);
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    createControls();
    tbLog("UNCONDITIONAL BUILD marker: toolbar constructed");
}

ToolbarWin32::~ToolbarWin32() {
    for (int id : m_btnBitmap.keys()) {
        if (m_btnBitmap[id]) DeleteObject(m_btnBitmap[id]);
        if (m_btnBitmapGrey[id]) DeleteObject(m_btnBitmapGrey[id]);
    }
    m_btnBitmap.clear();
    m_btnBitmapGrey.clear();
    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

int ToolbarWin32::heightPx() const {
    return dpi(viewer_settings::kToolbarBaseHeight);
}

void ToolbarWin32::setDpiScale(float scale) {
    // Remember each button's real icon, invalidate the cache, then rebuild at
    // the new pixel size (the build guard keys on the cached icon value).
    QHash<int, toolbar::Icon> actual = m_btnIcon;
    m_dpiScale = std::max(0.25f, std::min(scale, 8.0f));
    for (int id : actual.keys()) {
        m_btnIcon.remove(id);
        buildButtonBitmaps(id, actual[id]);
    }
    relayout();
}

void ToolbarWin32::createControls() {
    HFONT guiFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

struct Def { int id; toolbar::Control ctrl; toolbar::Icon icon; bool checkable; const wchar_t* tip; };
    const Def defs[] = {
        {ID_SIDEBAR,   toolbar::Control::SidebarToggle, toolbar::Icon::SidebarToggle, true,  L"Toggle outline sidebar"},
        {ID_PRINT,     toolbar::Control::Print,          toolbar::Icon::Print,         false, L"Print"},
        {ID_PREV,      toolbar::Control::PrevPage,       toolbar::Icon::Prev,          false, L"Previous page"},
        {ID_NEXT,      toolbar::Control::NextPage,       toolbar::Icon::Next,          false, L"Next page"},
        {ID_MODE,      toolbar::Control::ModeToggle,     toolbar::Icon::ModePaged,     true,  L"Toggle paged / continuous"},
        {ID_FIT,       toolbar::Control::FitButton,      toolbar::Icon::FitPage,       false, L"Fit mode (manual / page / width)"},
        {ID_ROT_L,     toolbar::Control::RotateLeft,     toolbar::Icon::RotateLeft,    false, L"Rotate left"},
        {ID_ROT_R,     toolbar::Control::RotateRight,    toolbar::Icon::RotateRight,   false, L"Rotate right"},
        {ID_ZOOM_OUT,  toolbar::Control::ZoomOut,        toolbar::Icon::ZoomOut,       false, L"Zoom out"},
        {ID_ZOOM_IN,   toolbar::Control::ZoomIn,         toolbar::Icon::ZoomIn,        false, L"Zoom in"},
        {ID_FIND_PREV, toolbar::Control::FindPrev,       toolbar::Icon::FindPrev,      false, L"Previous match"},
        {ID_FIND_NEXT, toolbar::Control::FindNext,       toolbar::Icon::FindNext,      false, L"Next match"},
        {ID_MATCH_CASE, toolbar::Control::MatchCase,     toolbar::Icon::MatchCase,     true,  L"Match case"},
        {ID_COPY,      toolbar::Control::Copy,           toolbar::Icon::Copy,          false, L"Copy: text selection arrives in a future change"},
    };
    for (const Def& d : defs) {
        Btn b{};
        b.id = d.id;
        b.control = d.ctrl;
        b.icon = d.icon;
        b.checkable = d.checkable;
        b.widthPx = buttonH();
        const HWND h = CreateWindowExW(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON,
            0, 0, b.widthPx, buttonH(),
            m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(d.id)),
            GetModuleHandleW(nullptr), nullptr);
        // Track intended checked state for toggle-style buttons ourselves (the
        // owner-drawn system checkbox toggling proved unreliable).
        if (d.checkable)
            m_manualCheck[d.id] = false;
        m_controls[d.id] = h;
        m_btnIcon[d.id] = d.icon;
        buildButtonBitmaps(d.id, d.icon);
        m_ctrlId[d.ctrl] = d.id;
        if (h) {
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(guiFont), TRUE);
            // Buttons may transiently take focus on click; the presenter hands
            // focus back to the viewer after each command so hotkeys recover.
            SetPropW(h, L"ToolbarTip", const_cast<wchar_t*>(d.tip));
        }
    }

    m_ctrlId[toolbar::Control::PageBox] = ID_PAGE_EDIT;
    m_ctrlId[toolbar::Control::PageCount] = ID_PAGE_STATIC;
    m_ctrlId[toolbar::Control::FindBox] = ID_FIND_EDIT;
    m_ctrlId[toolbar::Control::FindStatus] = ID_FIND_STATIC;

    m_controls[ID_PAGE_EDIT] = addEdit(ID_PAGE_EDIT, dpi(40)); // ~5 digits
    m_controls[ID_FIND_EDIT] = addEdit(ID_FIND_EDIT, dpi(120));
    m_controls[ID_PAGE_STATIC] = addStatic(ID_PAGE_STATIC, dpi(46));
    m_controls[ID_FIND_STATIC] = addStatic(ID_FIND_STATIC, dpi(78));

    for (int id : {ID_PAGE_EDIT, ID_FIND_EDIT, ID_PAGE_STATIC, ID_FIND_STATIC})
        SendMessageW(m_controls[id], WM_SETFONT, reinterpret_cast<WPARAM>(guiFont), TRUE);

    if (HWND h = m_controls.value(ID_PAGE_EDIT, nullptr))
        SetWindowTextW(h, L"1");
    if (HWND h = m_controls.value(ID_FIND_EDIT, nullptr))
        SetWindowTextW(h, L"");

    // Subclass both edit controls to commit on Enter / focus leave.
    for (int id : {ID_PAGE_EDIT, ID_FIND_EDIT}) {
        LRESULT oldProc = GetWindowLongPtrW(m_controls[id], GWLP_WNDPROC);
        SetWindowLongPtrW(m_controls[id], GWLP_USERDATA, oldProc);
        SetWindowLongPtrW(m_controls[id], GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(editProc));
    }

    layout();
    createTooltips();
}

void ToolbarWin32::createTooltips() {
    m_tooltip = CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr,
                                 WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                 0, 0, 0, 0, m_hwnd, nullptr,
                                 GetModuleHandleW(nullptr), nullptr);
    if (!m_tooltip)
        return;
    for (int id : m_controls.keys()) {
        HWND h = m_controls[id];
        if (!h)
            continue;
        wchar_t* tip = static_cast<wchar_t*>(GetPropW(h, L"ToolbarTip"));
        if (!tip)
            continue;
        TOOLINFOW ti = {};
        ti.cbSize = TTTOOLINFOW_V1_SIZE;
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.uId = reinterpret_cast<UINT_PTR>(h);
        ti.hwnd = m_hwnd;
        ti.lpszText = tip;
        SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    }
}

HWND ToolbarWin32::addEdit(int id, int widthPx, bool rightAlign) {
    // Single-line edits vertically center the text automatically. rightAlign is
    // a no-op here (ES_RIGHT only affects multiline); kept for API symmetry.
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT;
    HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                             style, 0, 0, widthPx, editH(), m_hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    m_controls[id] = h;
    return h;
}

HWND ToolbarWin32::addStatic(int id, int widthPx) {
    // SS_CENTERIMAGE centers the label vertically so it lines up with the
    // page/edit boxes instead of hugging the top of the strip.
    HWND h = CreateWindowExW(0, L"STATIC", L"",
                             WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                             0, 0, widthPx, editH(), m_hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    m_controls[id] = h;
    return h;
}

// Returns true if the bitmap was (re)built because the icon actually changed.
bool ToolbarWin32::buildButtonBitmaps(int id, toolbar::Icon icon) {
    const int sx = iconSizePx();
    // Rebuild when either the intended icon changed OR any stored bitmap is
    // missing. Uses explicit contains(), never the .value() default-compare
    // (which returns the requested icon when the key is absent, silently
    // skipping the rebuild — the root-cause of icons never repainting).
    if (m_btnIcon.contains(id) && m_btnIcon[id] == icon &&
        m_btnBitmap[id] && m_btnBitmapGrey[id] != nullptr)
        return false;
    m_btnIcon[id] = icon;
    if (HBITMAP b = m_btnBitmap.value(id)) { DeleteObject(b); m_btnBitmap.remove(id); }
    if (HBITMAP b = m_btnBitmapGrey.value(id)) { DeleteObject(b); m_btnBitmapGrey.remove(id); }
    const QImage normal = toolbar::makeIcon(icon, sx);
    m_btnBitmap[id] = imageToIconBitmap(normal, false);
    m_btnBitmapGrey[id] = imageToIconBitmap(normal, true);
    return true;
}

void ToolbarWin32::setIcon(toolbar::Control c, toolbar::Icon icon) {
    const int id = m_ctrlId.value(c, 0);
    if (!id)
        return;
    tbLog("setIcon ctrl=%d id=%d icon=%d", (int)c, id, (int)icon);
    if (!buildButtonBitmaps(id, icon)) {
        tbLog("setIcon ctrl=%d ic against already-set bitmap (no rebuild)", (int)c);
        return; // icon unchanged; nothing to repaint
    }
    // The bitmap changed: invalidate the WHOLE strip so every owner-drawn
    // child repaints with the new bitmap (some children don't redraw when
    // only they are invalidated).
    InvalidateRect(m_hwnd, nullptr, FALSE);
    tbLog("setIcon ctrl=%d DID rebuild + invalidate strip", (int)c);
}

void ToolbarWin32::setEnabled(toolbar::Control c, bool on) {
    const int id = m_ctrlId.value(c, 0);
    if (!id)
        return;
    HWND h = m_controls.value(id);
    if (!h)
        return;
    // Only repaint when the enabled state actually flips; calling this on every
    // navigation/zoom refresh would redraw the whole strip.
    if (m_btnEnabled.value(id, true) == on)
        return;
    m_btnEnabled[id] = on;
    EnableWindow(h, on);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ToolbarWin32::setChecked(toolbar::Control c, bool on) {
    const int id = m_ctrlId.value(c, 0);
    if (!id)
        return;
    // Track our own checked state (buttons are push-button style now; the
    // presenter owns the true on/off for toggle buttons).
    m_manualCheck[id] = on;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ToolbarWin32::setText(toolbar::Control c, const QString& text) {
    const int id = m_ctrlId.value(c, 0);
    if (!id)
        return;
    if (HWND h = m_controls.value(id))
        SetWindowTextW(h, reinterpret_cast<const wchar_t*>(text.utf16()));
}

void ToolbarWin32::setEditText(toolbar::Control c, const QString& text) {
    // Only touch the control when the text changed; setting it every refresh
    // repaints the box (flicker) and fires EN_CHANGE even on no-op.
    const int id = m_ctrlId.value(c, 0);
    if (!id)
        return;
    HWND h = m_controls.value(id);
    if (!h)
        return;
    const QString cur = textOfEdit(h);
    if (cur == text)
        return;
    SetWindowTextW(h, reinterpret_cast<const wchar_t*>(text.utf16()));
}

QString ToolbarWin32::editText(toolbar::Control c) const {
    const int id = m_ctrlId.value(c, 0);
    if (!id)
        return {};
    if (HWND h = m_controls.value(id))
        return textOfEdit(h);
    return {};
}

bool ToolbarWin32::isEditFocused() const {
    const HWND focus = ::GetFocus();
    return focus == m_controls.value(ID_PAGE_EDIT) || focus == m_controls.value(ID_FIND_EDIT);
}

void ToolbarWin32::restoreViewerFocus() const {
    const HWND focus = ::GetFocus();
    if (focus == m_controls.value(ID_PAGE_EDIT) || focus == m_controls.value(ID_FIND_EDIT))
        return; // an editing session keeps focus so typing is not interrupted
    if (!m_hwnd)
        return;
    const HWND viewer = GetParent(m_hwnd);
    if (viewer && focus != viewer)
        SetFocus(viewer);
}

LRESULT CALLBACK ToolbarWin32::editProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    const auto origProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_RETURN) {
            SendMessageW(GetParent(hwnd), WM_COMMAND, 0, reinterpret_cast<LPARAM>(hwnd));
            // After committing, give keyboard focus back to the viewer so
            // arrows/Esc/pgup/pgdown work again right away.
            SetFocus(GetParent(GetParent(hwnd)));
            return 0;
        }
        break;
    // NOTE: no WM_KILLFOCUS commit. Losing focus (e.g. when the user clicks a
    // toolbar button like Match case) must NOT re-run the find/page commit —
    // it would re-trigger the search on every button click.
    default:
        break;
    }
    return CallWindowProcW(origProc, hwnd, msg, wp, lp);
}

void ToolbarWin32::layout() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int H = rc.bottom;
    const int cw = buttonH();
    const int y = (H - cw) / 2;
    int x = step();

    const int slotWidth = cw;
    auto place = [&](int id, int w) -> int {
        if (HWND h = m_controls.value(id)) {
            int ch = cw;
            // Edits and the labels they sit beside share the edit-box height,
            // so the "/ N" label and the page count box align on one baseline.
            if (id == ID_PAGE_EDIT || id == ID_FIND_EDIT ||
                id == ID_PAGE_STATIC || id == ID_FIND_STATIC)
                ch = editH();
            const int ty = y + (cw - ch) / 2;
            MoveWindow(h, x, ty, w, ch, TRUE);
        }
        x += w + gap();
        return x;
    };

    place(ID_SIDEBAR, slotWidth);
    x += step();

    place(ID_PRINT, slotWidth);
    x += step();

    place(ID_PREV, slotWidth);
    place(ID_PAGE_EDIT, dpi(40));
    place(ID_PAGE_STATIC, dpi(46));
    place(ID_NEXT, slotWidth);
    x += step();

    place(ID_MODE, slotWidth);
    place(ID_FIT, slotWidth);
    x += step();

    place(ID_ROT_L, slotWidth);
    place(ID_ROT_R, slotWidth);
    place(ID_ZOOM_OUT, slotWidth);
    place(ID_ZOOM_IN, slotWidth);
    x += step();

    place(ID_FIND_PREV, slotWidth);
    place(ID_FIND_EDIT, dpi(120));
    place(ID_FIND_NEXT, slotWidth);
    place(ID_MATCH_CASE, slotWidth);
    // Copy directly after Match case, no group spacer.
    place(ID_COPY, slotWidth);
    place(ID_FIND_STATIC, dpi(78));   // "n / m" counter at the rightmost end
}

void ToolbarWin32::relayout() { layout(); }

void ToolbarWin32::onEditCommit(int id) {
    if (id == ID_PAGE_EDIT)
        presenter()->onGoToPageCommitted(editText(toolbar::Control::PageBox));
    else if (id == ID_FIND_EDIT)
        presenter()->onFindCommitted(editText(toolbar::Control::FindBox));
}

void ToolbarWin32::onCommand(int id) {
    tbLog("onCommand id=%d", id);
    toolbar::ToolbarPresenter* p = presenter();
    if (!p) {
        tbLog("onCommand id=%d NO-PRESENTER", id);
        return;
    }
    switch (id) {
    case ID_PREV:       p->onPrevPage(); break;
    case ID_NEXT:       p->onNextPage(); break;
    case ID_MODE:       p->onModeToggled(); break;
    case ID_FIT:        p->onFitCycled(); break;
    case ID_ROT_L:      p->onRotateLeft(); break;
    case ID_ROT_R:      p->onRotateRight(); break;
    case ID_ZOOM_OUT:   p->onZoomOut(); break;
    case ID_ZOOM_IN:    p->onZoomIn(); break;
    case ID_FIND_PREV:  p->onFindPrev(); break;
    case ID_FIND_NEXT:  p->onFindNext(); break;
    case ID_MATCH_CASE: {
        // Push-button toggle: flip our tracked state and feed the presenter.
        const bool next = !m_manualCheck.value(ID_MATCH_CASE, false);
        p->onMatchCaseToggled(next);
        break;
    }
    case ID_PRINT:      p->onPrint(); break;
    case ID_COPY:       p->onCopy(); break;
    case ID_SIDEBAR: {
        // Push-button toggle: the handler toggles visibility; refreshState
        // re-syncs the checked state from sidebarVisible().
        p->onSidebarToggled();
        break;
    }
    default: break;
    }
}

void ToolbarWin32::drawButton(HDC hDC, const DRAWITEMSTRUCT& dis) {
    const int id = static_cast<int>(dis.CtlID);
    RECT rc = dis.rcItem;
    // Owner-draw buttons don't mirror BM_SETCHECK into ODS_CHECKED; read our
    // own tracked toggle state instead.
    const bool checked = m_manualCheck.value(id, false);
    const bool pressed = (dis.itemState & ODS_SELECTED) != 0;
    const bool enabled = (dis.itemState & ODS_DISABLED) == 0;
    const bool focused = (dis.itemState & ODS_FOCUS) != 0;

    COLORREF fill = GetSysColor(COLOR_BTNFACE);
    if (checked && enabled)
        fill = RGB(0xE4, 0xEF, 0xFB);   // very light checked tint
    HBRUSH br = CreateSolidBrush(fill);
    FillRect(hDC, &rc, br);
    DeleteObject(br);

    if (pressed)
        DrawEdge(hDC, &rc, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
    else if (checked && enabled) {
        // Checked: a thin 1px highlight ring only — no sunken edge, keeps the
        // button visually light.
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x9A, 0xBE, 0xE0));
        HGDIOBJ oldPen = SelectObject(hDC, pen);
        HGDIOBJ oldBr = SelectObject(hDC, GetStockObject(NULL_BRUSH));
        Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hDC, oldBr);
        SelectObject(hDC, oldPen);
        DeleteObject(pen);
    }
    else if (focused)
        DrawFocusRect(hDC, &rc);

    HBITMAP bmp = enabled ? m_btnBitmap.value(id) : m_btnBitmapGrey.value(id);
    if (bmp) {
        BITMAP bi;
        GetObjectW(bmp, sizeof(bi), &bi);
        const int cx = (rc.right - rc.left - bi.bmWidth) / 2;
        const int cy = (rc.bottom - rc.top - bi.bmHeight) / 2;
        HDC mem = CreateCompatibleDC(hDC);
        HGDIOBJ old = SelectObject(mem, bmp);
        BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        AlphaBlend(hDC, rc.left + cx, rc.top + cy, bi.bmWidth, bi.bmHeight, mem, 0, 0, bi.bmWidth, bi.bmHeight, blend);
        SelectObject(mem, old);
        DeleteDC(mem);
    }
}

LRESULT CALLBACK ToolbarWin32::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<ToolbarWin32*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self)
        return self->handleMsg(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT ToolbarWin32::handleMsg(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND: {
        const WORD wId = LOWORD(wp);
        tbLog("WM_COMMAND wId=%d lp=%p", (int)wId, lp);
        if (!lp) {
            // BUTTON controls notify their parent with wParam = control ID and
            // lParam = 0 (BN_CLICKED etc.). Route straight to the command.
            onCommand(wId);
            restoreViewerFocus();
            return 0;
        }
        const HWND child = reinterpret_cast<HWND>(lp);
        // The edit controls report their own notifications (EN_CHANGE,
        // EN_UPDATE, EN_SETFOCUS...) while the user types or the presenter
        // syncs text; only the editProc-stamped commit (wParam == 0) may act.
        if (child == m_controls.value(ID_PAGE_EDIT) ||
            child == m_controls.value(ID_FIND_EDIT)) {
            if (HIWORD(wp) != 0)
                return 0; // native edit notification; ignore
            const int ctlId = (child == m_controls.value(ID_PAGE_EDIT)) ? ID_PAGE_EDIT : ID_FIND_EDIT;
            onEditCommit(ctlId);
            restoreViewerFocus();
            return 0;
        }
        onCommand(wId);
        restoreViewerFocus();
        return 0;
    }
    case WM_SETFOCUS:
        // The strip itself never keeps keyboard focus (only the edit boxes do);
        // hand it back to the viewer so hotkeys keep working.
        SetFocus(GetParent(m_hwnd));
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            // I-beam over the text edits (they accept caret input), arrow
            // everywhere else on the strip.
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(m_hwnd, &pt);
            const HWND under = ChildWindowFromPoint(m_hwnd, pt);
            if (under == m_controls.value(ID_PAGE_EDIT) ||
                under == m_controls.value(ID_FIND_EDIT)) {
                SetCursor(LoadCursor(nullptr, IDC_IBEAM));
                return TRUE;
            }
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (dis && dis->CtlType == ODT_BUTTON) {
            tbLog("WM_DRAWITEM btn id=%d checked=%d", (int)dis->CtlID,
                  (dis->itemState & ODS_CHECKED) ? 1 : 0);
            drawButton(dis->hDC, *dis);
        }
        return TRUE;
    }
    case WM_PAINT: {
        // Repainting the strip must also redraw the owner-drawn child buttons,
        // otherwise invalidating just the parent leaves them stale. Invalidate
        // all children so their WM_DRAWITEM fires.
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        // paint our own background
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));
        EndPaint(m_hwnd, &ps);
        // force children to redraw
        const auto handles = m_controls.values();
        for (HWND h : handles) {
            if (h && IsWindow(h))
                InvalidateRect(h, nullptr, TRUE);
        }
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, GetSysColorBrush(COLOR_BTNFACE));
        return 1;
    }
    case WM_SIZE:
        layout();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

#endif // Q_OS_WIN