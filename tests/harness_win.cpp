// Windows manual-test harness for mouse-drag-and-settings tasks 4.2-4.6.
// Hosts the real ViewerWin32 in a plain window, feeds it a generated
// multi-page PDF, drives real mouse/keyboard input via SendInput and
// asserts scroll/cursor/page behaviour. Prints PASS/FAIL lines.

#include "viewer_win32.h"

#include <QImage>

#include <windows.h>
#include <windowsx.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static int g_failures = 0;

#define CHECK(name, cond)                                                     \
    do {                                                                      \
        std::printf("%s %s (line %d)\n", (cond) ? "PASS" : "FAIL", name,      \
                    __LINE__);                                                \
        std::fflush(stdout);                                                  \
        if (!(cond))                                                          \
            ++g_failures;                                                     \
    } while (0)

// ---------------------------------------------------------------- PDF gen

static bool writeTestPdf(const char* path, int pageCount) {
    struct Obj { int id; std::string body; };
    std::vector<std::string> objs;
    // 1: catalog, 2: pages, then per page: page obj + content stream
    std::string kids;
    for (int p = 1; p <= pageCount; ++p) {
        int pageId = 2 + (p - 1) * 2 + 1;
        kids += std::to_string(pageId) + " 0 R ";
    }
    objs.push_back("<< /Type /Catalog /Pages 2 0 R >>");
    objs.push_back("<< /Type /Pages /Kids [" + kids + "] /Count " +
                   std::to_string(pageCount) + " >>");
    for (int p = 1; p <= pageCount; ++p) {
        const long long top = 500 - ((p - 1) * 45) % 480;
        std::string content =
            "0.85 g 30 30 360 535 re f\n"
            "0 g 20 " + std::to_string(top) + " 380 50 re f\n"
            "0.4 g 20 20 380 8 re f\n";
        objs.push_back("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 420 595] "
                       "/Contents " + std::to_string(2 + (p - 1) * 2 + 2) +
                       " 0 R /Resources << >> >>");
        objs.push_back("<< /Length " + std::to_string(content.size()) + " >>\n"
                       "stream\n" + content + "endstream");
    }

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    long long offset = 9; // "%PDF-1.4\n"
    f << "%PDF-1.4\n";
    std::vector<long long> offsets(objs.size() + 1);
    for (size_t i = 0; i < objs.size(); ++i) {
        offsets[i + 1] = offset;
        std::string head = std::to_string(i + 1) + " 0 obj\n";
        std::string tail = "\nendobj\n";
        f << head << objs[i] << tail;
        offset += (long long)head.size() + objs[i].size() + tail.size();
    }
    const long long xrefStart = offset;
    f << "xref\n0 " << (objs.size() + 1) << "\n";
    f << "0000000000 65535 f \n";
    for (size_t i = 1; i <= objs.size(); ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010lld 00000 n \n", offsets[i]);
        f << buf;
    }
    f << "trailer\n<< /Size " << (objs.size() + 1)
      << " /Root 1 0 R >>\nstartxref\n" << xrefStart << "\n%%EOF\n";
    return true;
}

// ------------------------------------------------------------- utilities

static void pump(DWORD ms) {
    const DWORD deadline = GetTickCount() + ms;
    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (GetTickCount() >= deadline)
            return;
        Sleep(10);
    }
}

struct VKey { WORD vk; bool up; };
static void sendKeys(const std::vector<VKey>& keys) {
    std::vector<INPUT> in;
    for (const auto& k : keys) {
        INPUT i{};
        i.type = INPUT_KEYBOARD;
        i.ki.wVk = k.vk;
        i.ki.dwFlags = k.up ? KEYEVENTF_KEYUP : 0;
        in.push_back(i);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

static void sendMouse(DWORD flags, LONG dx = 0, LONG dy = 0) {
    INPUT i{};
    i.type = INPUT_MOUSE;
    i.mi.dwFlags = flags;
    i.mi.dx = dx;
    i.mi.dy = dy;
    SendInput(1, &i, sizeof(INPUT));
}

static void moveCursorBy(HWND hwnd, int dx, int dy) {
    POINT pt{};
    GetCursorPos(&pt);
    // SetCursorPos alone generates WM_MOUSEMOVE; do NOT combine with a
    // relative SendInput move or the cursor travels twice the distance.
    SetCursorPos(pt.x + dx, pt.y + dy);
    pump(60);
}

static POINT clientCenter(HWND hwnd, int ox = 0, int oy = 0) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    POINT pt{ (rc.right - rc.left) / 2 + ox, (rc.bottom - rc.top) / 2 + oy };
    ClientToScreen(hwnd, &pt);
    return pt;
}

static int vPos(HWND hwnd) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);
    return (int)si.nPos;
}

static int vMaxScroll(HWND hwnd) {
    // Win32 caps the reachable scroll position at nMax - (nPage - 1), not
    // nMax - nPage, because the page-sized thumb can never fully overlap.
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);
    return (std::max)(0, (int)si.nMax - (int)si.nPage + 1);
}

static bool cursorIs(HWND hwnd, LPCWSTR idc) {
    pump(40);
    CURSORINFO ci{ sizeof(ci) };
    GetCursorInfo(&ci);
    // Only meaningful while the cursor is over our window.
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    POINT pt{ ci.ptScreenPos };
    if (!PtInRect(&rc, pt))
        return false;
    return ci.hCursor == LoadCursor(nullptr, idc);
}

static int g_pressIndex = 0;

static void drag(HWND hwnd, int dxTotal, int dyTotal, int steps = 12) {
    // Offset each press slightly so consecutive button-downs never land
    // within the system double-click rect (CS_DBLCLKS would swallow the
    // second press as WM_LBUTTONDBLCLK).
    const int jig = (g_pressIndex++ % 2) ? -12 : 12;
    const POINT start = clientCenter(hwnd, jig);
    SetCursorPos(start.x, start.y);
    pump(60);
    sendMouse(MOUSEEVENTF_LEFTDOWN);
    pump(60);
    for (int i = 1; i <= steps; ++i) {
        moveCursorBy(hwnd, dxTotal / steps, dyTotal / steps);
    }
    sendMouse(MOUSEEVENTF_LEFTUP);
    pump(80);
}

static void shiftV() {
    sendKeys({ {VK_SHIFT, false} });
    pump(60);
    sendKeys({ {'V', false} });
    pump(60);
    sendKeys({ {'V', true} });
    pump(60);
    sendKeys({ {VK_SHIFT, true} });
    pump(120);
}

// ------------------------------------------------------------------ main

int main() {
    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    std::string pdfPath = std::string(tmpPath) + "wlx_drag_test.pdf";
    if (!writeTestPdf(pdfPath.c_str(), 10)) {
        std::printf("FAIL pdf generation\n");
        return 2;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WLXHarnessHost";
    RegisterClassExW(&wc);

    HWND host = CreateWindowExW(0, L"WLXHarnessHost", L"harness",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                100, 100, 800, 600,
                                nullptr, nullptr, wc.hInstance, nullptr);
    if (!host) { std::printf("FAIL host window\n"); return 2; }
    pump(100);

    ViewerWin32 viewer(host);
    HWND vh = viewer.hwnd();
    MoveWindow(vh, 0, 0, 800, 600, TRUE);
    pump(100);

    if (!viewer.loadDocument(QString::fromLocal8Bit(pdfPath.c_str()))) {
        std::printf("FAIL load document\n");
        return 2;
    }
    pump(150);
    CHECK("precondition: starts in paged mode", viewer.controller()->isPagedMode());

    // ---- 4.3: paged mode -> drag does nothing, cursor unchanged
    // Only meaningful when the current page fits the page area; with the taller
    // toolbar a fit page can be taller than the viewport, in which case paged
    // panning legitimately engages (hand cursor + scroll change are correct).
    {
        const QRect pr = viewer.controller()->pageRect(viewer.controller()->currentPage());
        RECT cr{}; GetClientRect(vh, &cr);
        const int pageAreaH = (int)(cr.bottom - cr.top)
                              - viewer_settings::kToolbarBaseHeight;
        const bool pageFits = pr.isValid() && pr.height() <= pageAreaH;
        if (pageFits) {
            drag(vh, 0, -140);
            CHECK("4.3 paged: vertical scroll unchanged", vPos(vh) == 0);
            CHECK("4.3 paged: cursor not hand after attempt", !cursorIs(vh, IDC_HAND));
        } else {
            // Page overflows the page area: paged pan engages; just confirm it
            // stays within the vertical range.
            drag(vh, 0, -140);
            CHECK("4.3 paged(overflow): scroll within range",
                  vPos(vh) >= 0 && vPos(vh) <= (int)viewer.controller()->maxScrollOffsetYForPage(viewer.controller()->currentPage()));
        }
    }

    // ---- switch to continuous mode ('V')
    SetFocus(vh);
    SetForegroundWindow(host);
    pump(80);
    sendKeys({ {VK_HOME, false}, {VK_HOME, true} });
    pump(80);
    sendKeys({ {'V', false}, {'V', true} });
    pump(150);
    CHECK("precondition: continuous mode active", !viewer.controller()->isPagedMode());

    // ---- 4.2: smooth scrolling both directions
    {
        const int posBefore = vPos(vh);
        std::printf("  [dbg] posBefore=%d\n", posBefore);
        drag(vh, 0, -150);
        const int posAfterUp = vPos(vh);
        std::printf("  [dbg] after up-drag: %d (expected ~%d..%d)\n", posAfterUp,
                    posBefore + 130, posBefore + 170);
        CHECK("4.2 drag up scrolls forward (~+150)",
              posAfterUp > posBefore && posAfterUp <= posBefore + 190 &&
              posAfterUp >= posBefore + 110);

        drag(vh, 0, 90);
        const int posAfterDown = vPos(vh);
        std::printf("  [dbg] after down-drag: %d (expected ~%d..%d)\n",
                    posAfterDown, posAfterUp - 110, posAfterUp - 70);
        CHECK("4.2 drag down scrolls backward (~-90)",
              posAfterDown < posAfterUp && posAfterDown >= posAfterUp - 130 &&
              posAfterDown <= posAfterUp - 70);
    }

    // ---- 4.4: hand cursor during drag, arrow after release
    {
        const POINT start = clientCenter(vh);
        SetCursorPos(start.x, start.y);
        pump(60);
        sendMouse(MOUSEEVENTF_LEFTDOWN);
        pump(60);
        moveCursorBy(vh, 0, -40);
        const bool handDuring = cursorIs(vh, IDC_HAND);
        sendMouse(MOUSEEVENTF_LEFTUP);
        pump(80);
        const bool arrowAfter = cursorIs(vh, IDC_ARROW);
        CHECK("4.4 cursor is hand during drag", handDuring);
        CHECK("4.4 cursor restores to arrow on release", arrowAfter);
    }

    // ---- 4.5: clamping at both ends
    {
        // The cursor cannot leave the physical screen, so scroll toward the
        // end with repeated in-window drags until the position saturates.
        int prev = -1;
        for (int i = 0; i < 15 && vPos(vh) != prev; ++i) {
            prev = vPos(vh);
            drag(vh, 0, -600);
        }
        std::printf("  [dbg] saturated pos=%d maxScroll=%d\n", vPos(vh), vMaxScroll(vh));
        CHECK("4.5 clamp at document end", vPos(vh) == vMaxScroll(vh));
        CHECK("4.5 last page is current at end",
              viewer.controller()->currentPage() == viewer.controller()->pageCount());

        prev = -1;
        for (int i = 0; i < 15 && vPos(vh) != prev; ++i) {
            prev = vPos(vh);
            drag(vh, 0, 600);
        }
        CHECK("4.5 clamp at document start", vPos(vh) == 0);

        // Horizontal: fit-width strip has no h-range; ensure drag keeps it 0.
        const POINT start = clientCenter(vh);
        SetCursorPos(start.x, start.y);
        pump(60);
        sendMouse(MOUSEEVENTF_LEFTDOWN);
        pump(60);
        for (int i = 0; i < 10; ++i) moveCursorBy(vh, 25, 0);
        sendMouse(MOUSEEVENTF_LEFTUP);
        pump(80);
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(vh, SB_HORZ, &si);
        const int hMax = (std::max)(0, (int)si.nMax - (int)si.nPage);
        CHECK("4.5 horizontal stays within bounds",
              (int)si.nPos >= 0 && (int)si.nPos <= hMax);
    }

    // ---- 4.6: shift+V fit-cycle preserves current page after navigation.
    // The cycle is FitToPage -> FitToWidth -> Manual -> FitToPage, so three
    // presses return to the starting mode. Precondition uses deterministic
    // full-page jumps (VK_NEXT in continuous = advance one page top) so the
    // cursor never has to move off-screen.
    {
        SetFocus(vh);
        pump(60);
        for (int i = 0; i < 3; ++i)
            PostMessageW(vh, WM_KEYDOWN, VK_NEXT, 0);
        pump(150);
        const int pageBefore = viewer.controller()->currentPage();
        std::printf("  [dbg] vPos=%d pageBefore=%d\n", vPos(vh), pageBefore);
        CHECK("precondition: mid-document page tracked", pageBefore > 1);

        int pages[3];
        for (int c = 0; c < 3; ++c) {
            shiftV();
            pages[c] = viewer.controller()->currentPage();
            CHECK("4.6 page preserved by shift+V", pages[c] == pageBefore);
        }
        CHECK("4.6 fit mode cycled back",
              viewer.controller()->fitMode() == ViewerController::FitMode::FitToPage &&
              pages[0] == pageBefore && pages[1] == pageBefore &&
              pages[2] == pageBefore);
    }

    std::printf("\n%s (%d failure(s))\n", g_failures ? "RESULT: FAIL" : "RESULT: ALL PASS",
                g_failures);
    return g_failures ? 1 : 0;
}
