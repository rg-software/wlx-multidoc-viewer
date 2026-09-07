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

// Minimal single-page PDF with drawn text (Helvetica) so the selection layer
// finds real words. Coordinates in PDF units (y-up, origin bottom-left; the
// engine normalizes to y-down).
static bool writeTextTestPdf(const char* path) {
    std::string content =
        "BT\n"
        "/F1 24 Tf\n"
        "50 500 Td\n"
        "(Hello) Tj\n"
        "0 -32 Td\n"
        "(world) Tj\n"
        "ET\n";
    std::string font = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";

    std::string page =
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 420 595] /Contents 4 0 R "
        "/Resources << /Font << /F1 5 0 R >> >> >>";
    std::string stream = "<< /Length " + std::to_string(content.size()) + " >>\nstream\n" +
                         content + "endstream";

    std::vector<std::string> objs = {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        page,
        stream,
        font,
    };

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
        // Page area now subtracts the toolbar top; derive
        // it from the controller instead of assuming a fixed height.
        const int vhPx = viewer.controller()->pageAreaHeight();
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
        CHECK("B3 page 2 stacked below page 1 + gap",
              p1.y() == 0 && p2.y() == p1.height() + viewer_settings::kPageGap);
        const int canvasW = viewer.controller()->contentSize().width();
        // Ratio checks (robust to the toolbar/DPI chrome height changing the
        // absolute fit size): page 1 stays portrait, page 2 stays landscape.
        const double r1 = static_cast<double>(p1.width()) / p1.height();
        const double r2 = static_cast<double>(p2.width()) / p2.height();
        CHECK("B1 page 1 portrait aspect (w/h == 420/595)",
              std::abs(r1 - 420.0 / 595.0) < 0.003);
        CHECK("B2 page 2 landscape aspect (w/h == 595/420)",
              std::abs(r2 - 595.0 / 420.0) < 0.003);
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

    // ---------------- G) Two-page presentation: P key, unit pairing, scroll
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_double.pdf";
        if (!writeTestPdf(pdfPath.c_str(), 5, 420, 595)) {
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
        pump(100);

        // Post the real WM_KEYDOWN -> onKeyDown path (like B7/B8), optionally
        // holding Shift for the guard test.
        auto postKey = [vh](WORD vk) {
            PostMessageW(vh, WM_KEYDOWN, vk, 0);
            pump(200);
        };
        auto postShiftKey = [vh](WORD vk) {
            BYTE shiftedState[256] = {};
            GetKeyboardState(shiftedState);
            BYTE holdShift[256] = {};
            memcpy(holdShift, shiftedState, sizeof(holdShift));
            holdShift[VK_SHIFT] |= 0x80;
            SetKeyboardState(holdShift);
            PostMessageW(vh, WM_KEYDOWN, vk, 0);
            pump(150);
            SetKeyboardState(shiftedState);
        };
        auto pr = [&viewer](int p) { return viewer.controller()->pageRect(p); };
        const int gap = viewer_settings::kPageGap;

        // Single-mode geometry before any cycling: byte-for-byte regression
        // guard for when the cycle returns to Single.
        const QRect singleP1 = pr(1);
        const QRect singleP5 = pr(5);
        CHECK("G1 starts Single",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::Single);
        CHECK("G1a single p1 centered",
              singleP1.x() ==
                  (viewer.controller()->contentSize().width() - singleP1.width()) / 2);

        postKey('P');
        CHECK("G2 P -> Double",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::Double);
        CHECK("G3 mode untouched (still paged)", viewer.controller()->isPagedMode());
        CHECK("G4 current page resolves to unit first (1)",
              viewer.controller()->currentPage() == 1);

        // Unit (1,2): shared row, gap between members, unit centered as a whole.
        const QRect p1 = pr(1);
        const QRect p2 = pr(2);
        const int unitW = p1.width() + gap + p2.width();
        const int unitH = std::max(p1.height(), p2.height());
        CHECK("G5 unit members share the row (y==0)",
              p1.y() == 0 && p2.y() == 0);
        CHECK("G6 unit centered as a whole",
              p1.x() == (viewer.controller()->contentSize().width() - unitW) / 2);
        CHECK("G7 gap between members",
              p2.x() == p1.x() + p1.width() + gap);

        // Row 2 starts below the whole unit height.
        const QRect p3 = pr(3);
        CHECK("G8 row 2 offset = unit height + gap",
              p3.y() == unitH + gap);

        // Odd last page stands alone, centered.
        const int widest = viewer.controller()->contentSize().width();
        CHECK("G9 odd last page is a singleton unit",
              viewer.controller()->unitLast(5) == 5);
        CHECK("G10 lone last page centered",
              pr(5).x() == (widest - pr(5).width()) / 2);

        // Paged navigation steps unit to unit; goToPage resolves to unit first.
        viewer.controller()->nextPage();
        pump(40);
        CHECK("G11 paged nextPage jumps one unit (1 -> 3)",
              viewer.controller()->currentPage() == 3);
        viewer.controller()->goToPage(4);
        pump(40);
        CHECK("G12 goToPage(4) resolves to 3", viewer.controller()->currentPage() == 3);
        viewer.controller()->goToPage(2);
        pump(40);
        CHECK("G13 goToPage(2) resolves to 1", viewer.controller()->currentPage() == 1);
        viewer.controller()->goToPage(5);
        pump(40);
        CHECK("G14 goToPage(5) resolves to 5 (singleton unit)",
              viewer.controller()->currentPage() == 5);

        // Shift+P must not cycle.
        postShiftKey('P');
        CHECK("G15 Shift+P does not cycle",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::Double);
        CHECK("G16 Shift+P leaves page unchanged",
              viewer.controller()->currentPage() == 5);

        // Horizontal pan range covers the WHOLE unit (incl. the gap), and the
        // H-scrollbar exposes it (updateScrollBars' paged unit width). Land on
        // unit (1,2) so the range spans the pair, then zoom 2.0 makes it wider
        // than the viewport.
        viewer.controller()->goToPage(1);
        pump(40);
        viewer.controller()->setManualZoom(2.0f, 0);
        pump(120);
        const int unitW2 = pr(1).width() + gap + pr(2).width();
        CHECK("G17 unit H-pan range = unit width - viewport",
              viewer.controller()->maxScrollOffsetXForUnit(1) ==
                  std::max(0, unitW2 - viewer.controller()->pageAreaWidth()));
        PostMessageW(vh, WM_HSCROLL, MAKEWPARAM(SB_LINERIGHT, 0), 0);
        pump(60);
        SCROLLINFO hsi{};
        hsi.cbSize = sizeof(hsi);
        hsi.fMask = SIF_ALL;
        GetScrollInfo(vh, SB_HORZ, &hsi);
        std::printf("  [dbg] double-h: hPos=%d nMax=%d unitW2=%d pageAreaW=%d\n",
                    hPos(vh), hsi.nMax, unitW2, viewer.controller()->pageAreaWidth());
        std::fflush(stdout);
        CHECK("G18 paged H-scrollbar range spans the unit (incl. gap)",
              hsi.nMax == unitW2 - 1 && hsi.nMax > 0);
        CHECK("G19 H-pan in double moves within the unit range",
              hPos(vh) > 0 && hPos(vh) <= viewer.controller()->maxScrollOffsetXForUnit(1));
        viewer.controller()->setManualZoom(1.0f, 0);
        pump(80);
        PostMessageW(vh, WM_HSCROLL, MAKEWPARAM(SB_LEFT, 0), 0);
        pump(60);

        // Double -> DoubleWithCover. Page 5's cover-mode unit is (4,5), so the
        // resolved current page must be its unit first (4).
        viewer.controller()->goToPage(5);
        pump(40);
        postKey('P');
        CHECK("G20 P -> DoubleWithCover",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::DoubleWithCover);
        CHECK("G21 current page on cover-mode unit first (unitFirst(5)=4)",
              viewer.controller()->currentPage() == 4);
        CHECK("G22 cover page 1 is its own unit",
              viewer.controller()->unitFirst(1) == 1 &&
              viewer.controller()->unitLast(1) == 1);
        CHECK("G23 (2,3) pair in cover mode",
              viewer.controller()->unitFirst(2) == 2 &&
              viewer.controller()->unitLast(2) == 3);
        CHECK("G24 (4,5) pair in cover mode",
              viewer.controller()->unitFirst(4) == 4 &&
              viewer.controller()->unitLast(4) == 5);

        // Cover-mode geometry: page 1 alone centered; pairs lay out normally.
        const int widestCov = viewer.controller()->contentSize().width();
        const QRect c1 = pr(1);
        const QRect c2 = pr(2);
        const QRect c3 = pr(3);
        CHECK("G25 cover page 1 centered alone",
              c1.x() == (widestCov - c1.width()) / 2);
        CHECK("G26 cover page 1 sits above the pairs",
              c1.y() == 0 && c2.y() == c1.height() + gap);
        CHECK("G27 (2,3) share the row with the gap between",
              c3.y() == c2.y() && c3.x() == c2.x() + c2.width() + gap);
        CHECK("G28 pair (4,5) below (2,3) + gap",
              pr(4).y() == c2.y() + std::max(c2.height(), c3.height()) + gap);

        // Cover-aware navigation: 1 -> 2 -> 4 ...
        viewer.controller()->goToPage(3);
        pump(40);
        CHECK("G29 goToPage(3) resolves to 2", viewer.controller()->currentPage() == 2);
        viewer.controller()->nextPage();
        pump(40);
        CHECK("G30 cover nextPage 2 -> 4", viewer.controller()->currentPage() == 4);
        viewer.controller()->prevPage();
        pump(40);
        CHECK("G31 cover prevPage 4 -> 2", viewer.controller()->currentPage() == 2);
        viewer.controller()->prevPage();
        pump(40);
        CHECK("G32 cover prevPage 2 -> 1", viewer.controller()->currentPage() == 1);
        viewer.controller()->nextPage();
        pump(40);
        CHECK("G33 cover nextPage 1 -> 2", viewer.controller()->currentPage() == 2);

        // Cycle back to Single: geometry matches the original byte-for-byte.
        postKey('P');
        CHECK("G34 P -> Single",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::Single);
        CHECK("G35 returned to unit first", viewer.controller()->currentPage() == 2);
        CHECK("G36 Single geometry identical to the original",
              pr(1) == singleP1 && pr(5) == singleP5);

        // Fit-to-width fits the combined unit across the viewport width.
        postKey('P');
        CHECK("G37 P -> Double (again)",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::Double);
        CHECK("G38 mode preserved across presentation cycle",
              viewer.controller()->isPagedMode());
        viewer.controller()->cycleFitMode(0); // Manual -> FitToPage
        viewer.controller()->cycleFitMode(0); // FitToPage -> FitToWidth
        pump(120);
        const int fitUnitW = pr(1).width() + gap + pr(2).width();
        std::printf("  [dbg] fit-to-width unitW=%d pageAreaW=%d\n", fitUnitW,
                    viewer.controller()->pageAreaWidth());
        std::fflush(stdout);
        CHECK("G39 fit-to-width spans the whole unit",
              std::abs(fitUnitW - viewer.controller()->pageAreaWidth()) <= 2);
        viewer.controller()->cycleFitMode(0); // FitToWidth -> Manual(100%)
        pump(120);

        // Continuous double: unit members share a scroll offset; rows step by
        // the combined height; the P cycle in continuous re-targets correctly.
        viewer.controller()->goToPage(1);
        pump(40);
        viewer.controller()->toggleMode();
        pump(150);
        CHECK("G40 continuous mode active", !viewer.controller()->isPagedMode());
        CHECK("G41 presentation survives mode toggle",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::Double);
        const int rowTop = pr(1).y();
        CHECK("G42 unit members share the same scroll offset",
              viewer.controller()->scrollOffsetForPage(1) ==
                      viewer.controller()->scrollOffsetForPage(2) &&
              viewer.controller()->scrollOffsetForPage(1) == rowTop);
        CHECK("G43 next unit row = combined height + gap",
              viewer.controller()->scrollOffsetForPage(3) ==
                  rowTop + unitH + gap);

        // Double -> DoubleWithCover in continuous keeps the view on a unit
        // containing the current page (1 stays its own cover row at 0).
        postKey('P');
        CHECK("G44 P in continuous -> DoubleWithCover",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::DoubleWithCover);
        CHECK("G45 cover row scrolls to page 1's row",
              vPos(vh) == viewer.controller()->scrollOffsetForPage(1));
        postKey('P');
        CHECK("G46 P -> Single in continuous",
              viewer.controller()->pagePresentation() ==
                  ViewerState::PagePresentation::Single);
        CHECK("G47 single continuous keeps the page-anchored scroll",
              vPos(vh) == viewer.controller()->scrollOffsetForPage(1));

        // Fit-to-width from the cover must target the two-page spread, not the
        // singleton cover, so the next spread still fits the viewport width.
        viewer.controller()->toggleMode(); // continuous -> paged
        pump(60);
        postKey('P'); // Single -> Double
        pump(40);
        postKey('P'); // Double -> DoubleWithCover
        pump(40);
        viewer.controller()->goToPage(1);
        pump(40);
        CHECK("G48 cover is its own singleton unit",
              viewer.controller()->currentPage() == 1 &&
              viewer.controller()->unitLast(1) == 1);
        viewer.controller()->cycleFitMode(0); // Manual -> FitToPage
        viewer.controller()->cycleFitMode(0); // FitToPage -> FitToWidth
        pump(120);
        const int G48spreadW = pr(2).width() + gap + pr(3).width();
        std::printf("  [dbg] G48 spreadW=%d pageAreaW=%d coverW=%d\n",
                    G48spreadW, viewer.controller()->pageAreaWidth(),
                    pr(1).width());
        std::fflush(stdout);
        CHECK("G49 fit-to-width off the cover targets the spread",
              std::abs(G48spreadW - viewer.controller()->pageAreaWidth()) <= 2);
        CHECK("G50 cover renders narrower than the viewport",
              pr(1).width() < viewer.controller()->pageAreaWidth());
        viewer.controller()->goToPage(2);
        pump(60);
        CHECK("G51 spread unit does not overflow horizontally",
              viewer.controller()->maxScrollOffsetXForUnit(2) == 0);

        // Navigation buttons step whole screens; the last screen is reached while
        // currentPage < pageCount, so enablement is decided by hasNext/hasPrev.
        viewer.controller()->cycleFitMode(0); // FitToWidth -> Manual
        pump(40);
        viewer.controller()->goToPage(2);     // unit {2,3}
        pump(40);
        CHECK("G52 after {2,3} a next screen exists",
              viewer.controller()->hasNextPage());
        viewer.controller()->nextPage();
        pump(40);
        CHECK("G52b nextPage advances to the following unit {4,5}",
              viewer.controller()->currentPage() == 4);
        CHECK("G53 in a cover doc the {4,5} unit is the final screen",
              viewer.controller()->hasPrevPage() &&
              !viewer.controller()->hasNextPage());
        const bool g53bNoOp = viewer.controller()->nextPage();
        pump(40);
        CHECK("G53b nextPage from the final screen is a no-op",
              !g53bNoOp && viewer.controller()->currentPage() == 4);
        pump(40);
        CHECK("G54 final screen still has a previous screen",
              viewer.controller()->hasPrevPage());
        viewer.controller()->goToPage(1);     // cover unit {1}
        pump(40);
        CHECK("G55 cover: no previous screen, next exists",
              !viewer.controller()->hasPrevPage() &&
              !viewer.controller()->prevPage() &&
              viewer.controller()->hasNextPage());
        viewer.controller()->cyclePagePresentation(); // DoubleWithCover -> Single (paged)
        pump(40);
        viewer.controller()->goToPage(viewer.controller()->pageCount());
        pump(40);
        CHECK("G56 single paged last page has no next",
              !viewer.controller()->hasNextPage());
        viewer.controller()->goToPage(1);
        pump(40);
        CHECK("G57 single paged first page has no prev",
              !viewer.controller()->hasPrevPage());

        // Continuous: the toolbar's screen step must move whole units in a
        // double-page presentation (next from the cover hits {2,3}, from {2,3}
        // hits {4,5}, and the final unit is a no-op) while single-paged
        // continuous still steps raw pages.
        viewer.controller()->cyclePagePresentation(); // Single -> Double (paged)
        pump(40);
        viewer.controller()->cyclePagePresentation(); // Double -> DoubleWithCover (paged)
        pump(40);
        viewer.controller()->toggleMode(); // paged -> continuous
        pump(60);
        CHECK("G58 continuous cover prev is a no-op",
              viewer.controller()->scrollStepPage(1, -1) == 1);
        CHECK("G58b continuous cover next lands on {2,3}",
              viewer.controller()->scrollStepPage(1, +1) == 2);
        CHECK("G59 continuous {2,3} next lands on {4,5}",
              viewer.controller()->scrollStepPage(2, +1) == 4 &&
              viewer.controller()->scrollStepPage(3, +1) == 4);
        CHECK("G59b continuous {2,3} prev returns to the cover unit",
              viewer.controller()->scrollStepPage(2, -1) == 1 &&
              viewer.controller()->scrollStepPage(3, -1) == 1);
        CHECK("G60 continuous final unit {4,5} next is a no-op",
              viewer.controller()->scrollStepPage(4, +1) == 4 &&
              viewer.controller()->scrollStepPage(5, +1) == 5);
        CHECK("G60b continuous final unit prev steps back to {2,3}",
              viewer.controller()->scrollStepPage(5, -1) == 2);
        viewer.controller()->cyclePagePresentation(); // DoubleWithCover -> Single (continuous)
        pump(40);
        CHECK("G61 single continuous next/prev are raw page steps",
              viewer.controller()->scrollStepPage(1, +1) == 2 &&
              viewer.controller()->scrollStepPage(5, -1) == 4 &&
              viewer.controller()->scrollStepPage(1, -1) == 1 &&
              viewer.controller()->scrollStepPage(5, +1) == 5);

        DestroyWindow(host);
        pump(50);
    }

    // ---------------- E) Text selection (MuPDF path)
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_text.pdf";
        if (!writeTextTestPdf(pdfPath.c_str())) {
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

        CHECK("E1 page has selectable text (MuPDF)",
              viewer.controller()->pageHasText(1));
        const PageText pt = viewer.controller()->pageText(1);
        std::printf("  [dbg] E: %d words: ", (int)pt.words.size());
        for (const TextWord& w : pt.words)
            std::printf("'%s' ", w.text.toUtf8().constData());
        std::printf("\n");
        std::fflush(stdout);
        CHECK("E2 words extracted (>=2: 'Hello','world')", pt.words.size() >= 2);

        // Transform round-trip: page center maps to the page rect center on the
        // content canvas; a word bbox lands within the page's scaled rect.
        viewer.controller()->setManualZoom(1.0f, 0);
        pump(100);
        const QRect pr = viewer.controller()->pageRect(1);
        const QTransform t = viewer.controller()->pageTransform(1);
        const PageInfo pi = viewer.controller()->engine()->pageDimensions(1);
        QPointF pcenter = t.map(QPointF(pi.width / 2.0, pi.height / 2.0));
        QPointF rcenter(pr.x() + pr.width() / 2.0, pr.y() + pr.height() / 2.0);
        CHECK("E3 page center maps to canvas page-center",
              std::abs(pcenter.x() - rcenter.x()) < 2.0 &&
              std::abs(pcenter.y() - rcenter.y()) < 2.0);

        // Word 0 ("Hello") bbox maps inside the page rect on canvas.
        const QRectF wbbox = viewer.controller()->wordRectOnCanvas(1, 0);
        CHECK("E4 word rect inside page rect",
              wbbox.isValid() && pr.contains(wbbox.toRect()));

        // Hit-test: page top-left in canvas should hit no word; a point on a
        // word's bbox center should hit that word via inverse transform.
        const QRectF firstWord = pt.words[0].bbox;
        const QPointF firstWordCanvas = t.map(
            QPointF(firstWord.center().x(), firstWord.center().y()));
        const int hitIdx = viewer.controller()->wordAtCanvas(1, firstWordCanvas);
        CHECK("E5 hit-test at word center returns the word", hitIdx == 0);

        // Begin/update selection then read the copied text (char-level:
        // anchor at the start of word 0 through the end of the last word).
        viewer.controller()->beginSelection(1, 0, 0);
        viewer.controller()->updateSelection(1, (int)pt.words.size() - 1, -1);
        viewer.controller()->endSelection();
        const QString sel = viewer.controller()->selectedText();
        std::printf("  [dbg] E selectedText: '%s'\n", sel.toUtf8().constData());
        std::fflush(stdout);
        CHECK("E6 selectedText joins words", !sel.isEmpty());
        viewer.controller()->clearSelection();
        CHECK("E7 clearSelection empties text", viewer.controller()->selectedText().isEmpty());

        DestroyWindow(host);
        pump(50);
    }

    // ---------------- F) Win32 selection through the real mouse message path
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_text.pdf";
        if (!writeTextTestPdf(pdfPath.c_str())) {
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
        pump(100);

        const PageText pt = viewer.controller()->pageText(1);
        CHECK("F0 two words extracted", pt.words.size() >= 2);
        const QRect pr = viewer.controller()->pageRect(1);
        const QTransform t = viewer.controller()->pageTransform(1);

        // On-screen page origin in paged mode, mirroring onPaint: centered only when
// the page fits the page area, otherwise pinned to the area's top-left (0
// scroll). Client point = canvas point - pageRect.topLeft + on-screen origin
// (the exact inverse of ViewerWin32::clientToCanvas).
        const int topChrome = viewer_settings::kToolbarBaseHeight;
        const int bottomChrome = 0;
        RECT cr;
        GetClientRect(vh, &cr);
        const int viewW = cr.right;
        const int viewH = cr.bottom - topChrome - bottomChrome;
        const int prW = pr.width();
        const int prH = pr.height();
        const int ox = (prW <= viewW) ? (viewW - prW) / 2 : 0;
        const int oy = topChrome + ((prH <= viewH) ? (viewH - prH) / 2 : 0);

        // Down on the LEFT edge of word 0 (char 0), drag to the last word's
        // right edge, release. Char-level selection starts at the first char.
        const QRectF w0 = t.mapRect(pt.words[0].bbox);
        const QRectF wLast = t.mapRect(pt.words[pt.words.size() - 1].bbox);
        const int downX = (int)w0.left() + 2 - pr.x() + ox;
        const int downY = (int)w0.center().y() - pr.y() + oy;
        const int upX = (int)wLast.right() - pr.x() + ox;
        const int upY = (int)wLast.center().y() - pr.y() + oy;

        PostMessageW(vh, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(downX, downY));
        pump(40);
        PostMessageW(vh, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(upX, upY));
        pump(40);
        PostMessageW(vh, WM_LBUTTONUP, 0, MAKELPARAM(upX, upY));
        pump(80);

        const QString sel = viewer.controller()->selectedText();
        std::printf("  [dbg] F selectedText: '%s'\n", sel.toUtf8().constData());
        std::fflush(stdout);
        CHECK("F1 selection produced text through mouse path", !sel.isEmpty());
        CHECK("F2 selectedText contains first word", sel.contains("Hello"));
        CHECK("F3 hasSelection active", viewer.controller()->hasSelection());

        // Esc clears.
        PostMessageW(vh, WM_KEYDOWN, VK_ESCAPE, 0);
        pump(80);
        CHECK("F4 Esc clears selection", !viewer.controller()->hasSelection());

        DestroyWindow(host);
        pump(50);
    }

    std::printf("\n%s (%d failure(s))\n", g_failures ? "RESULT: FAIL" : "RESULT: ALL PASS",
                g_failures);
    return g_failures ? 1 : 0;
} // placeholder to keep ptrscan honest