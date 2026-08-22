// Continuous scrolling spec-validation harness for continuous-scroll-performance.
// Hosts the real ViewerWin32, feeds it generated PDFs (uniform long doc for the
// mid-document-reach test, alternating portrait/landscape for the mixed-size
// test) and asserts the virtual-canvas behavior. Prints PASS/FAIL lines.

#include "viewer_win32.h"

#include <QImage>

#include <windows.h>

#include <cstdio>
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

// Generates pageCount pages; every page MediaBox is [0 0 w h] (pass alternating
// w/h for a mixed-size document).
static bool writeTestPdf(const char* path, int pageCount, int w = 420, int h = 595,
                         bool alternate = false) {
    std::vector<std::string> objs;
    std::string kids;
    for (int p = 1; p <= pageCount; ++p) {
        int iw = (alternate && p % 2 == 0) ? h : w;
        int ih = (alternate && p % 2 == 0) ? w : h;
        int pageId = 2 + (p - 1) * 2 + 1;
        kids += std::to_string(pageId) + " 0 R ";
        std::string med = "[0 0 " + std::to_string(iw) + " " + std::to_string(ih) + "]";
        std::string content = "0.85 g 30 30 360 535 re f\n"
                              "0 g 20 500 380 50 re f\n";
        objs.push_back("<< /Type /Page /Parent 2 0 R /MediaBox " + med +
                       " /Contents " + std::to_string(2 + (p - 1) * 2 + 2) +
                       " 0 R /Resources << >> >>");
        objs.push_back("<< /Length " + std::to_string(content.size()) + " >>\n"
                       "stream\n" + content + "endstream");
    }
    objs.insert(objs.begin(), {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [" + kids + "] /Count " + std::to_string(pageCount) + " >>",
    });

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    long long offset = 9;
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
    f << "xref\n0 " << (objs.size() + 1) << "\n0000000000 65535 f \n";
    for (size_t i = 1; i <= objs.size(); ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010lld 00000 n \n", offsets[i]);
        f << buf;
    }
    f << "trailer\n<< /Size " << (objs.size() + 1) << " /Root 1 0 R >>\nstartxref\n"
      << xrefStart << "\n%%EOF\n";
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
static void sendKey(WORD vk) {
    INPUT in[2]{};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = vk;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vk; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
    pump(80);
}

static int vPos(HWND hwnd) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);
    return (int)si.nPos;
}

static int hPos(HWND hwnd) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_HORZ, &si);
    return (int)si.nPos;
}

static int vMaxScroll(HWND hwnd) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);
    return (std::max)(0, (int)si.nMax - (int)si.nPage + 1);
}

static void setContinuous(ViewerWin32& viewer, HWND vh) {
    for (int i = 0; i < 8; ++i) {
        if (!viewer.controller()->isPagedMode())
            return;
        SetForegroundWindow(GetParent(vh));
        SetFocus(vh);
        pump(60);
        sendKey('V');
        pump(150);
    }
}

int main() {
    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WLXHarnessHostScroll";
    RegisterClassExW(&wc);

    // ---------------- A) Long uniform doc: mid-document reachability
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_long.pdf";
        const int pages = 4000; // content height > old 1.5M px cap
        if (!writeTestPdf(pdfPath.c_str(), pages, 420, 595)) {
            std::printf("FAIL pdf gen\n");
            return 2;
        }

        HWND host = CreateWindowExW(0, L"WLXHarnessHostScroll", L"harness",
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                    120, 120, 800, 600,
                                    nullptr, nullptr, wc.hInstance, nullptr);
        pump(100);
        ViewerWin32 viewer(host);
        HWND vh = viewer.hwnd();
        MoveWindow(vh, 0, 0, 800, 600, TRUE);
        pump(100);
        if (!viewer.loadDocument(QString::fromLocal8Bit(pdfPath.c_str()))) {
            std::printf("FAIL load\n");
            return 2;
        }
        pump(150);
viewer.controller()->setManualZoom(1.0f, 0);
        pump(80);
        viewer.controller()->toggleMode();
        pump(200);
        CHECK("A0 continuous mode active", !viewer.controller()->isPagedMode());
        pump(80);

        const qint64 contentH = viewer.controller()->contentSize().height();
        const int vhPx = 600 - ViewerController::kInfoPanelHeight; // ~578 client
        RECT cr{}; GetClientRect(vh, &cr);
        std::printf("  [dbg] contentH=%lld vh=%d maxScroll=%d isOpen=%d pageCount=%d cs=(%d,%d) clientH=%d vMaxScroll=%d\n",
                    (long long)contentH, vhPx, viewer.controller()->maxScrollOffset(),
                    (int)viewer.controller()->hasDocument(),
                    viewer.controller()->pageCount(),
                    viewer.controller()->contentSize().width(),
                    viewer.controller()->contentSize().height(),
                    (int)(cr.bottom - cr.top),
                    vMaxScroll(vh));
        CHECK("A1 scroll range equals contentHeight - viewport",
              viewer.controller()->maxScrollOffset() ==
                  (int)std::max<long long>(0, contentH - vhPx));
        // Old strip cap (1.5M) would have bounded scroll reach to ~1.5M px;
        // a 4000-page doc at zoom 1.0 is ~2.37M px tall, so the middle is
        // reachable only via the virtual canvas.
        CHECK("A2 content height exceeds old strip cap",
              contentH > 1500000);
        // Scrollbar (already verified via A1 range math; A3 also confirms the
        // reachable thumb track is within a scrollbar-track epsilon of the
        // controller's max scroll).
        CHECK("A3 scrollbar reachable track ~= controller max scroll",
              std::abs(vMaxScroll(vh) - viewer.controller()->maxScrollOffset()) <= 256);

        // Mapping to the middle page: ~half the content => ~page 2000.
        const int middleScroll = viewer.controller()->maxScrollOffset() / 2;
        const int middlePage = viewer.controller()->pageAtScrollOffset(middleScroll);
        std::printf("  [dbg] middleScroll=%d -> page %d (of %d)\n",
                    middleScroll, middlePage, pages);
        CHECK("A4 middle scroll maps near page 2000",
              middlePage > 1800 && middlePage < 2200);

        // Bottom of the document: at max scroll the last page must be reported
        // and the last page must actually be reachable.
        const int maxY = viewer.controller()->maxScrollOffset();
        CHECK("A5 document end reached at max scroll",
              viewer.controller()->pageAtScrollOffset(maxY) == pages);
        CHECK("A6 last page fits the viewport at end",
              viewer.controller()->pageRect(pages).top() <= maxY &&
              viewer.controller()->pageRect(pages).bottom() > maxY);

        // Regression: near the bottom, when the penultimate page is at the top
        // of the viewport, painting must start from it (firstPageAtScroll), and
        // the current page reports the penultimate page while it is dominant
        // (most-visible semantics), only switching to the last page when it
        // takes over the viewport.
        const int inPenultimate =
            viewer.controller()->scrollOffsetForPage(pages - 1) + 50;
        CHECK("A9 paint starts at penultimate page near bottom",
              viewer.controller()->firstPageAtScroll(inPenultimate) == pages - 1);
        CHECK("A10 current page reports penultimate while dominant",
              viewer.controller()->pageAtScrollOffset(inPenultimate) == pages - 1);
        CHECK("A11 current page reports last page at its top",
              viewer.controller()->pageAtScrollOffset(
                  viewer.controller()->scrollOffsetForPage(pages)) == pages);
        CHECK("A12 current page reports last page at max scroll",
              viewer.controller()->pageAtScrollOffset(maxY) == pages);

        // A page jump of exactly one page in continuous mode advances by that
        // page's geometry (height + gap).
        const int gap = viewer_settings::kPageGap;
        const int page3Top = viewer.controller()->scrollOffsetForPage(3);
        const int page4Top = viewer.controller()->scrollOffsetForPage(4);
        CHECK("A8 page jump = pageHeight + gap",
              page4Top - page3Top == viewer.controller()->pageRect(3).height() + gap);

        DestroyWindow(host);
        pump(50);
    }

    // ---------------- B) Mixed page sizes: real per-page layout + anchor
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_mixed.pdf";
        if (!writeTestPdf(pdfPath.c_str(), 8, 420, 595, /*alternate=*/true)) {
            std::printf("FAIL pdf gen\n");
            return 2;
        }
        HWND host = CreateWindowExW(0, L"WLXHarnessHostScroll", L"harness",
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                    120, 120, 800, 600,
                                    nullptr, nullptr, wc.hInstance, nullptr);
        pump(100);
        ViewerWin32 viewer(host);
        HWND vh = viewer.hwnd();
        MoveWindow(vh, 0, 0, 800, 600, TRUE);
        pump(100);
        if (!viewer.loadDocument(QString::fromLocal8Bit(pdfPath.c_str()))) {
            std::printf("FAIL load\n");
            return 2;
        }
        pump(80);
viewer.controller()->setManualZoom(1.0f, 0);
        pump(80);
        viewer.controller()->toggleMode();
        pump(200);
        CHECK("B0 continuous mode active", !viewer.controller()->isPagedMode());
        pump(80);

        const QRect p1 = viewer.controller()->pageRect(1); // portrait
        const QRect p2 = viewer.controller()->pageRect(2); // landscape
        std::printf("  [dbg] p1=(%d,%d %dx%d) p2=(%d,%d %dx%d)\n",
                    p1.x(), p1.y(), p1.width(), p1.height(),
                    p2.x(), p2.y(), p2.width(), p2.height());
        CHECK("B1 page 1 keeps portrait geometry (420x595)",
              p1.width() == 420 && p1.height() == 595);
        CHECK("B2 page 2 keeps landscape geometry (595x420)",
              p2.width() == 595 && p2.height() == 420);
        CHECK("B3 page 2 stacked below page 1 + gap",
              p1.y() == 0 && p2.y() == 595 + viewer_settings::kPageGap);
        const int canvasW = viewer.controller()->contentSize().width();
        CHECK("B4 each page centered within the canvas width",
              p1.x() == (canvasW - p1.width()) / 2 && p2.x() == (canvasW - p2.width()) / 2);

        // Zoom anchor: zooming keeps the top-of-viewport page the same.
        const int scrollMid = 2 * 595 + 40; // inside page 3-ish
        const int topBefore = viewer.controller()->firstPageAtScroll(scrollMid);
        const int newScroll = viewer.controller()->zoomIn(scrollMid);
        std::printf("  [dbg] scrollMid=%d topBefore=%d newScroll=%d newTop=%d\n",
                    scrollMid, topBefore, newScroll,
                    viewer.controller()->firstPageAtScroll(newScroll));
        CHECK("B5 zoom preserves viewport anchor page (mixed sizes)",
              viewer.controller()->firstPageAtScroll(newScroll) == topBefore);

        // Rotation anchor across a mixed doc: the guarantee is that the page
        // at the top of the viewport is preserved (most-visible can legitimately
        // shift after a reflow).
        viewer.controller()->setManualZoom(1.0f, 0);
        pump(80);
        const int rotTopBefore = viewer.controller()->firstPageAtScroll(scrollMid);
        const int rotScroll = viewer.controller()->rotateCw(scrollMid);
        CHECK("B6 rotate preserves top-of-viewport anchor page",
              viewer.controller()->firstPageAtScroll(rotScroll) == rotTopBefore);

        DestroyWindow(host);
        pump(50);
    }

    // ---------------- B7) 'V' key: paged -> continuous preserves current page
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_toggle.pdf";
        const int pageCount = 20;
        if (!writeTestPdf(pdfPath.c_str(), pageCount, 420, 595)) {
            std::printf("FAIL pdf gen\n");
            return 2;
        }
        HWND host = CreateWindowExW(0, L"WLXHarnessHostScroll", L"harness",
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                    120, 120, 800, 600,
                                    nullptr, nullptr, wc.hInstance, nullptr);
        pump(100);
        ViewerWin32 viewer(host);
        HWND vh = viewer.hwnd();
        MoveWindow(vh, 0, 0, 800, 600, TRUE);
        pump(100);
        if (!viewer.loadDocument(QString::fromLocal8Bit(pdfPath.c_str()))) {
            std::printf("FAIL load\n");
            return 2;
        }
        pump(80);
        viewer.controller()->setManualZoom(1.0f, 0);
        pump(80);
        CHECK("B7a starts paged", viewer.controller()->isPagedMode());

        // Land on page 5 in paged mode (both via controller and the real key
        // path once focused) then press 'V'.
        viewer.controller()->goToPage(5);
        pump(80);
        CHECK("B7b paged current page = 5", viewer.controller()->currentPage() == 5);

        SetForegroundWindow(host);
        SetFocus(vh);
        pump(80);
        // Drive the REAL 'V' key path deterministically by posting WM_KEYDOWN
        // directly to the viewer window (bypasses OS focus flakiness but goes
        // through handleMsg -> onKeyDown, the exact code being tested).
        auto postVKey = [vh]() { PostMessageW(vh, WM_KEYDOWN, 'V', 0); };
        postVKey();
        pump(200);
        if (viewer.controller()->isPagedMode()) {
            postVKey();
            pump(200);
        }
        CHECK("B7c switched to continuous", !viewer.controller()->isPagedMode());
        CHECK("B7d current page preserved = 5 (not reset to 1)",
              viewer.controller()->currentPage() == 5);
        CHECK("B7e scroll offset lands on page 5's top",
              vPos(vh) == viewer.controller()->scrollOffsetForPage(5));

        // And back: continuous -> paged keeps the tracked page.
        postVKey();
        pump(200);
        if (!viewer.controller()->isPagedMode()) {
            postVKey();
            pump(200);
        }
        CHECK("B7f switched back to paged", viewer.controller()->isPagedMode());
        CHECK("B7g page preserved after return to paged",
              viewer.controller()->currentPage() == 5);

        DestroyWindow(host);
        pump(50);
    }

    // ---------------- B8) Shift+V cycles fit modes; V alone toggles mode
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_fit.pdf";
        if (!writeTestPdf(pdfPath.c_str(), 6, 420, 595)) {
            std::printf("FAIL pdf gen\n");
            return 2;
        }
        HWND host = CreateWindowExW(0, L"WLXHarnessHostScroll", L"harness",
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                    120, 120, 800, 600,
                                    nullptr, nullptr, wc.hInstance, nullptr);
        pump(100);
        ViewerWin32 viewer(host);
        HWND vh = viewer.hwnd();
        MoveWindow(vh, 0, 0, 800, 600, TRUE);
        pump(100);
        if (!viewer.loadDocument(QString::fromLocal8Bit(pdfPath.c_str()))) {
            std::printf("FAIL load\n");
            return 2;
        }
        pump(80);
        // Leave the default fit mode (FitToPage) intact — setManualZoom would
        // force Manual. Instead just confirm the default after load.
        CHECK("B8a default fit mode = FitToPage",
              viewer.controller()->fitMode() == ViewerController::FitMode::FitToPage);

        // Shift+V: hold Shift via SetKeyboardState (GetKeyState, which
        // onKeyDown reads, reflects this thread's state), post V, THEN pump so
        // the message is dispatched while Shift is still held; restore after.
        auto postShiftV = [vh]() {
            BYTE shiftedState[256] = {};
            GetKeyboardState(shiftedState);
            BYTE holdShift[256] = {};
            memcpy(holdShift, shiftedState, sizeof(holdShift));
            holdShift[VK_SHIFT] |= 0x80;
            SetKeyboardState(holdShift);
            PostMessageW(vh, WM_KEYDOWN, 'V', 0);
            pump(150);
            SetKeyboardState(shiftedState);
        };

        // Shift+V cycles FitToPage -> FitToWidth -> Manual(100%) -> FitToPage.
        postShiftV(); pump(150);
        CHECK("B8b Shift+V #1 -> FitToWidth",
              viewer.controller()->fitMode() == ViewerController::FitMode::FitToWidth);
        postShiftV(); pump(150);
        CHECK("B8c Shift+V #2 -> Manual(100%)",
              viewer.controller()->fitMode() == ViewerController::FitMode::Manual &&
              viewer.controller()->zoom() > 0.999f && viewer.controller()->zoom() < 1.001f);
        postShiftV(); pump(150);
        CHECK("B8d Shift+V #3 -> back to FitToPage",
              viewer.controller()->fitMode() == ViewerController::FitMode::FitToPage);
        CHECK("B8e Shift+V did NOT toggle paged/continuous",
              viewer.controller()->isPagedMode());

        DestroyWindow(host);
        pump(50);
    }

    // ---------------- B9) Paged mode horizontal panning when page overflows
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_wide.pdf";
        if (!writeTestPdf(pdfPath.c_str(), 4, 420, 595)) {
            std::printf("FAIL pdf gen\n");
            return 2;
        }
        HWND host = CreateWindowExW(0, L"WLXHarnessHostScroll", L"harness",
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                    120, 120, 800, 600,
                                    nullptr, nullptr, wc.hInstance, nullptr);
        pump(100);
        ViewerWin32 viewer(host);
        HWND vh = viewer.hwnd();
        MoveWindow(vh, 0, 0, 800, 600, TRUE);
        pump(100);
        if (!viewer.loadDocument(QString::fromLocal8Bit(pdfPath.c_str()))) {
            std::printf("FAIL load\n");
            return 2;
        }
        pump(80);
        viewer.controller()->setManualZoom(2.0f, 0); // 840px wide > ~784px client
        pump(200);
        CHECK("B9a paged, H-range > 0 (page overflows)",
              viewer.controller()->isPagedMode() &&
              viewer.controller()->maxScrollOffsetXForPage(viewer.controller()->currentPage()) > 0);

        // Paged-mode H-scrollbar must be present with a usable range.
        SCROLLINFO hsi{};
        hsi.cbSize = sizeof(hsi);
        hsi.fMask = SIF_ALL;
        GetScrollInfo(vh, SB_HORZ, &hsi);
        CHECK("B9a1 paged H-scrollbar has range (page overflows)",
              hsi.nMax > 0);

        // Verify horizontal panning plumbing deterministically via the H-scrollbar
        // (SB_LINERIGHT moves m_scrollX through onHScroll -> clamp -> repaint,
        // the exact path a mouse drag or scrollbar arrow uses). The raw
        // SendInput mouse path is unreliable under automation (no foreground
        // capture), as seen above.
        auto postHScroll = [vh](WORD code) {
            PostMessageW(vh, WM_HSCROLL, MAKEWPARAM(code, 0), 0);
            pump(50);
        };
        for (int i = 0; i < 8; ++i) postHScroll(SB_LINERIGHT);
        std::printf("  [dbg] paged-h: hPos=%d xRange=%d\n", hPos(vh),
                    viewer.controller()->maxScrollOffsetXForPage(viewer.controller()->currentPage()));
        std::fflush(stdout);
        CHECK("B9b horizontal pan moved the page (m_scrollX > 0)",
              hPos(vh) > 0);
        CHECK("B9c clamped to page H-range",
              hPos(vh) <= viewer.controller()->maxScrollOffsetXForPage(
                               viewer.controller()->currentPage()));

        // Full-range: scrolling left to the end clamps at the page's overflow.
        PostMessageW(vh, WM_HSCROLL, MAKEWPARAM(SB_LEFT, 0), 0);
        pump(50);
        for (int i = 0; i < 200; ++i) postHScroll(SB_LINERIGHT);
        CHECK("B9d horizontal pan clamps at page max",
              hPos(vh) == viewer.controller()->maxScrollOffsetXForPage(
                              viewer.controller()->currentPage()));

        // Vertical paging-pan availability: a tall page (zoom makes it taller than
// the viewport) must be vertically drag-pannable in paged mode. The V
// scrollbar stays page-jump (drag is the pan mechanism), so we verify the
// model exposes the vertical overflow; the drag clamps to it exactly like the
// horizontal path (B9b-B9d, which shares onDragMove).
        {
            viewer.controller()->setManualZoom(2.0f, 0); // 1190px tall > ~578px
            pump(200);
            const int yRange = viewer.controller()->maxScrollOffsetYForPage(
                                   viewer.controller()->currentPage());
            std::printf("  [dbg] B9v: yRange=%d vPos=%d\n", yRange, vPos(vh));
            std::fflush(stdout);
            CHECK("B9e tall page has vertical paged overflow",
                  yRange > 0);
            CHECK("B9g V-scrollbar remains page-jump (nPos = currentPage-1)",
                  vPos(vh) == viewer.controller()->currentPage() - 1);
        }

        DestroyWindow(host);
        pump(50);
    }

    std::printf("\n%s (%d failure(s))\n", g_failures ? "RESULT: FAIL" : "RESULT: ALL PASS",
                g_failures);
    return g_failures ? 1 : 0;
} // placeholder to keep ptrscan honest