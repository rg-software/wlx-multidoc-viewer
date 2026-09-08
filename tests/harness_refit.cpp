// Refit fit-zoom-on-navigation spec-validation harness
// (fix-refit-fit-zoom-on-navigation). Hosts the real ViewerWin32, feeds it
// generated PDFs with page 1 kept WIDER than the pages that follow (the
// mixed-size condition that reproduces the two-tap PgUp/PgDn bug) and asserts:
//   3.1) In paged fit-to-page every page fits the page area after navigation
//        (no unit requires an in-page scroll), so one PgDn/PgUp lands on the
//        next/prev page.
//   3.2) Paged fit-to-width fits the narrow current unit to the viewport width;
//        continuous fit-to-width still targets the document-wide widest row.
//   3.3) Uniform-size documents: navigating pages does not change the zoom
//        (refit is a no-op) and pages still fit.

#include "viewer_win32.h"

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
// (same generator as harness_scroll.cpp)

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

static WNDCLASSEXW registerHostClass(const wchar_t* className) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = className;
    RegisterClassExW(&wc);
    return wc;
}

// Loads a generated PDF in a fresh host + ViewerWin32 sized 800x600.
// Returns nullptr on failure (host destroyed).
static ViewerWin32* loadPdf(const WNDCLASSEXW& wc, const std::string& path,
                            HWND* outHost) {
    HWND host = CreateWindowExW(0, wc.lpszClassName, L"harness",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                120, 120, 800, 600,
                                nullptr, nullptr, wc.hInstance, nullptr);
    pump(100);
    ViewerWin32* viewer = new ViewerWin32(host);
    HWND vh = viewer->hwnd();
    MoveWindow(vh, 0, 0, 800, 600, TRUE);
    pump(100);
    if (!viewer->loadDocument(QString::fromLocal8Bit(path.c_str()))) {
        std::printf("FAIL load: %s\n", path.c_str());
        delete viewer;
        DestroyWindow(host);
        return nullptr;
    }
    pump(150);
    *outHost = host;
    return viewer;
}

int main() {
    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);

    const WNDCLASSEXW wc = registerHostClass(L"WLXHarnessHostRefit");

    // ---------------- A) 3.1: mixed-size doc, paged fit-to-page, one-tap advance
    // Page 1 is LANDSCAPE (595x420) and pages 2+ alternate to portrait
    // (420x595): the open-time fit is anchored on the wide page, so without the
    // per-page refit every portrait page overflows vertically and takes two
    // PgDn presses (the reported bug).
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_refit_mixed.pdf";
        if (!writeTestPdf(pdfPath.c_str(), 8, 595, 420, /*alternate=*/true)) {
            std::printf("FAIL pdf gen\n");
            return 2;
        }
        HWND host = nullptr;
        ViewerWin32* viewer = loadPdf(wc, pdfPath, &host);
        if (!viewer)
            return 2;
        HWND vh = viewer->hwnd();

        CHECK("A0 paged by default", viewer->controller()->isPagedMode());
        CHECK("A1 default fit mode = FitToPage",
              viewer->controller()->fitMode() == ViewerController::FitMode::FitToPage);

        // Anchor page 1 must be the wide one (positioning bug repro). Then every
        // page must fit the page area after a per-page refit, so no unit
        // requires an in-page scroll (maxY <= kPageBlockOverlap).
        CHECK("A1a anchor (page 1) is the wide/landscape page",
              viewer->controller()->pageRect(1).width() >
                  viewer->controller()->pageRect(2).width());
        bool allFit = true;
        for (int p = 1; p <= viewer->controller()->pageCount(); ++p) {
            viewer->controller()->goToPage(p);
            pump(40);
            const int first = viewer->controller()->unitFirst(p);
            const int maxY =
                viewer->controller()->maxScrollOffsetYForUnit(first);
            std::printf("  [dbg] A page %d: unit first=%d maxScrollY=%d pageAreaH=%d\n",
                        p, first, maxY, viewer->controller()->pageAreaHeight());
            std::fflush(stdout);
            if (viewer->controller()->unitRequiresVerticalScroll(first))
                allFit = false;
        }
        CHECK("A2 every mixed-size page fits under per-page fit-to-page refit",
              allFit);

        // One PgDn per key press advances exactly one page: walk pages 1..N
        // through the real VK_NEXT path, expecting currentPage to increment by
        // 1 each time (no extra scroll step stuck on the same page).
        viewer->controller()->goToPage(1);
        pump(40);
        bool oneTap = true;
        for (int expect = 2; expect <= viewer->controller()->pageCount(); ++expect) {
            PostMessageW(vh, WM_KEYDOWN, VK_NEXT, 0);
            pump(120);
            std::printf("  [dbg] A PgDn expect=%d got=%d\n", expect,
                        viewer->controller()->currentPage());
            std::fflush(stdout);
            if (viewer->controller()->currentPage() != expect)
                oneTap = false;
        }
        CHECK("A3 one PgDn press advances exactly one page in paged fit-to-page",
              oneTap);

        // ... and one PgUp per press goes back one page.
        bool oneTapUp = true;
        for (int expect = viewer->controller()->pageCount() - 1; expect >= 1; --expect) {
            PostMessageW(vh, WM_KEYDOWN, VK_PRIOR, 0);
            pump(120);
            std::printf("  [dbg] A PgUp expect=%d got=%d\n", expect,
                        viewer->controller()->currentPage());
            std::fflush(stdout);
            if (viewer->controller()->currentPage() != expect)
                oneTapUp = false;
        }
        CHECK("A4 one PgUp press returns exactly one page", oneTapUp);

        delete viewer;
        DestroyWindow(host);
        pump(50);
    }

    // ---------------- B) 3.2: fit-to-width targets the current unit in paged
    // mode but the widest row in continuous mode
    // Page 1 is PORTRAIT (420x595) here, so the paged fit-to-width unit is the
    // narrow page and the tall page provides the legitimate in-page scroll.
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_refit_mixed2.pdf";
        if (!writeTestPdf(pdfPath.c_str(), 4, 420, 595, /*alternate=*/true)) {
            std::printf("FAIL pdf gen\n");
            return 2;
        }
        HWND host = nullptr;
        ViewerWin32* viewer = loadPdf(wc, pdfPath, &host);
        if (!viewer)
            return 2;

        // Paged fit-to-width: navigate to a portrait page and confirm its
        // fitted width equals the page area (the narrow current unit is the fit
        // target, NOT the document's wide landscape row).
        viewer->controller()->cycleFitMode(0); // FitToPage -> FitToWidth
        pump(120);
        viewer->controller()->goToPage(1);
        pump(40);
        const int narrowW = viewer->controller()->pageRect(1).width();
        std::printf("  [dbg] B paged fit-to-width portrait1W=%d pageAreaW=%d "
                    "landscape2W=%d\n",
                    narrowW, viewer->controller()->pageAreaWidth(),
                    viewer->controller()->pageRect(2).width());
        std::fflush(stdout);
        CHECK("B1 paged fit-to-width fits the narrow current unit",
              std::abs(narrowW - viewer->controller()->pageAreaWidth()) <= 2);

        // A tall portrait page at that width-fit zoom genuinely overflows
        // vertically (real content below the fold), so PgDn scrolls within it
        // first. That is documented behavior — assert the overflow is real, not
        // the spurious 1-2px rounding case (which would need an extra tap).
        viewer->controller()->goToPage(3); // portrait page at same zoom
        pump(40);
        const int tallOverflow =
            viewer->controller()->maxScrollOffsetYForUnit(3);
        std::printf("  [dbg] B portrait page3 overflowY=%d\n", tallOverflow);
        std::fflush(stdout);
        CHECK("B2 paged fit-to-width portrait page scrolls within it (real overflow)",
              tallOverflow > 0);

        // Continuous fit-to-width still targets the doc-wide widest row: the
        // landscape row fills the page area while the narrow portrait page is
        // smaller. (Toggling modes does not refit, so re-enter the fit cycle in
        // continuous to recompute for maxRowWidth.)
        viewer->controller()->toggleMode(); // paged -> continuous
        pump(150);
        viewer->controller()->setManualZoom(1.0f, 0);
        pump(80);
        viewer->controller()->cycleFitMode(0); // Manual -> FitToPage
        viewer->controller()->cycleFitMode(0); // FitToPage -> FitToWidth (cont.)
        pump(120);
        viewer->controller()->goToPage(1);
        pump(60);
        const int cNarrowW = viewer->controller()->pageRect(1).width();
        viewer->controller()->goToPage(2);
        pump(60);
        const int cWideW = viewer->controller()->pageRect(2).width();
        std::printf("  [dbg] B continuous fit-to-width narrowW=%d wideW=%d pageAreaW=%d\n",
                    cNarrowW, cWideW, viewer->controller()->pageAreaWidth());
        std::fflush(stdout);
        CHECK("B3 continuous fit-to-width widest row fits the page area",
              std::abs(cWideW - viewer->controller()->pageAreaWidth()) <= 2);
        CHECK("B4 continuous fit-to-width keeps narrow units narrower",
              cNarrowW < cWideW);

        delete viewer;
        DestroyWindow(host);
        pump(50);
    }

    // ---------------- C) 3.3: uniform-size doc — refit is a zoom no-op
    {
        std::string pdfPath = std::string(tmpPath) + "wlx_refit_uniform.pdf";
        if (!writeTestPdf(pdfPath.c_str(), 12, 420, 595)) {
            std::printf("FAIL pdf gen\n");
            return 2;
        }
        HWND host = nullptr;
        ViewerWin32* viewer = loadPdf(wc, pdfPath, &host);
        if (!viewer)
            return 2;

        const float zoomStart = viewer->controller()->zoom();
        bool zoomStable = true;
        for (int p = 1; p <= viewer->controller()->pageCount(); ++p) {
            viewer->controller()->nextPage();
            pump(30);
            if (std::abs(viewer->controller()->zoom() - zoomStart) > 1e-5f)
                zoomStable = false;
        }
        std::printf("  [dbg] C uniform zoomStart=%f finalZoom=%f\n",
                    zoomStart, viewer->controller()->zoom());
        std::fflush(stdout);
        CHECK("C1 uniform doc: navigating paged fit-to-page does not change zoom",
              zoomStable);
        CHECK("C2 uniform doc: every page still fits (single-tap available)",
              !viewer->controller()->unitRequiresVerticalScroll(
                  viewer->controller()->unitFirst(viewer->controller()->currentPage())));

        delete viewer;
        DestroyWindow(host);
        pump(50);
    }

    std::printf("\n%s (%d failure(s))\n", g_failures ? "RESULT: FAIL" : "RESULT: ALL PASS",
                g_failures);
    return g_failures ? 1 : 0;
}