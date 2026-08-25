#include "viewercontroller.h"
#include "printcoordinator.h"
#include "print_win32.h"

#include <QImage>
#include <algorithm>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>

namespace {

constexpr DWORD kMaxPageRanges = 1;

// 24bpp bottom-up DIB from an RGB image, ready for StretchDIBits.
struct Dib {
    std::vector<uchar> data;
    BITMAPINFOHEADER bi = {};
    int width = 0;
    int height = 0;
    bool build(const QImage& img) {
        if (img.isNull())
            return false;
        QImage rgb = img.convertToFormat(QImage::Format_RGB888);
        width = rgb.width();
        height = rgb.height();
        const int stride = ((width * 3 + 3) / 4) * 4;
        data.resize(static_cast<size_t>(stride) * height);

        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = width;
        bi.biHeight = height; // bottom-up
        bi.biPlanes = 1;
        bi.biBitCount = 24;
        bi.biCompression = BI_RGB;

        for (int y = 0; y < height; ++y) {
            const uchar* srcRow = rgb.constScanLine(height - 1 - y);
            auto* dst = data.data() + static_cast<size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                dst[x * 3 + 0] = srcRow[x * 3 + 2]; // B
                dst[x * 3 + 1] = srcRow[x * 3 + 1]; // G
                dst[x * 3 + 2] = srcRow[x * 3 + 0]; // R
            }
        }
        return true;
    }
};

} // namespace

bool printDocumentWin32(HWND hParent, ViewerController* controller) {
    if (!controller || !controller->hasDocument())
        return false;

    const int pageCount = controller->pageCount();
    if (pageCount <= 0)
        return false;

    PRINTPAGERANGE pageRange = {1, static_cast<DWORD>(pageCount)};

    PRINTDLGEX pdx = {};
    pdx.lStructSize = sizeof(pdx);
    pdx.hwndOwner = hParent;
    pdx.Flags = PD_ALLPAGES | PD_PAGENUMS | PD_NOSELECTION | PD_NOCURRENTPAGE;
    pdx.nMinPage = 1;
    pdx.nMaxPage = static_cast<DWORD>(pageCount);
    pdx.nPageRanges = 1;
    pdx.nMaxPageRanges = kMaxPageRanges;
    pdx.lpPageRanges = &pageRange;
    pdx.nStartPage = START_PAGE_GENERAL;

    const HRESULT hr = PrintDlgEx(&pdx);
    if (FAILED(hr)) {
        MessageBoxW(hParent, L"Printing could not start.\n\nThe print dialog failed to initialize.",
                    L"Print", MB_OK | MB_ICONERROR);
        return false;
    }
    if (hr != S_OK || pdx.dwResultAction == PD_RESULT_CANCEL) {
        // Cancelled: nothing was sent, viewer state untouched.
        return false;
    }

    // Resolve the chosen page range (PrintDlgEx fills it back into the range).
    DWORD first = 1;
    DWORD last = pdx.nMaxPage;
    if (pdx.Flags & PD_PAGENUMS) {
        first = pageRange.nFromPage;
        last = pageRange.nToPage;
    }
    first = std::max<DWORD>(1, first);
    last = std::min<DWORD>(last, pdx.nMaxPage);

    QVector<int> pages;
    // Copies: expand into the pass list so the spooler receives exact copies.
    const DWORD copies = std::max<DWORD>(1, pdx.nCopies);
    for (DWORD c = 0; c < copies; ++c)
        for (DWORD p = first; p <= last; ++p)
            pages.append(static_cast<int>(p));

    // Create the printer DC from the dialog's chosen DEVMODE.
    DEVMODEW* devMode = nullptr;
    if (pdx.hDevMode)
        devMode = static_cast<DEVMODEW*>(GlobalLock(pdx.hDevMode));

    LPCWSTR printerName = nullptr;
    if (pdx.hDevNames) {
        auto* dn = static_cast<DEVNAMES*>(GlobalLock(pdx.hDevNames));
        if (dn)
            printerName = reinterpret_cast<const wchar_t*>(reinterpret_cast<const uchar*>(dn) + dn->wDeviceOffset);
    }

    HDC hdc = CreateDCW(L"WINSPOOL", printerName ? printerName : L"", nullptr,
                        devMode ? devMode : nullptr);
    if (pdx.hDevMode && devMode) GlobalUnlock(pdx.hDevMode);
    if (pdx.hDevNames) GlobalUnlock(pdx.hDevNames);

    if (!hdc) {
        MessageBoxW(hParent, L"No printer is available.", L"Print", MB_OK | MB_ICONERROR);
        return false;
    }

    // Printable (device) area after accounting for the physical margins.
    const int offsetX = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    const int offsetY = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    const int prnW = GetDeviceCaps(hdc, HORZRES);
    const int prnH = GetDeviceCaps(hdc, VERTRES);
    const int maxW = std::max(1, prnW - 2 * offsetX);
    const int maxH = std::max(1, prnH - 2 * offsetY);

    DOCINFOW di = {};
    di.cbSize = sizeof(di);
    di.lpszDocName = L"wlx-multidoc-viewer";

    if (StartDocW(hdc, &di) <= 0) {
        DeleteDC(hdc);
        MessageBoxW(hParent, L"Printing failed: the spooler rejected the document.",
                    L"Print", MB_OK | MB_ICONERROR);
        return false;
    }

    auto sink = [&](int, const QImage& img) -> bool {
        Dib buf;
        if (!buf.build(img))
            return false;
        // Fit on the printable area without cropping (StretchDIBits scales).
        const double sx = static_cast<double>(maxW) / buf.width;
        const double sy = static_cast<double>(maxH) / buf.height;
        const double s = std::min({1.0, sx, sy});
        const int dstW = static_cast<int>(buf.width * s);
        const int dstH = static_cast<int>(buf.height * s);
        const int dstX = offsetX + (prnW - dstW) / 2;
        const int dstY = offsetY + (prnH - dstH) / 2;

        if (StartPage(hdc) <= 0)
            return false;
        StretchDIBits(hdc, dstX, dstY, dstW, dstH,
                      0, 0, buf.width, buf.height,
                      buf.data.data(),
                      reinterpret_cast<BITMAPINFO*>(&buf.bi), DIB_RGB_COLORS, SRCCOPY);
        if (EndPage(hdc) <= 0)
            return false;
        return true;
    };

    PrintCoordinator coordinator;
    bool ok = coordinator.start(controller->engine(), controller->rotation(),
                                QSize(maxW, maxH), pages, sink, nullptr, nullptr);
    if (ok)
        coordinator.join();

    if (EndDoc(hdc) <= 0) {
        DeleteDC(hdc);
        MessageBoxW(hParent, L"Printing failed while the job was being submitted.",
                    L"Print", MB_OK | MB_ICONERROR);
        return false;
    }
    DeleteDC(hdc);
    return true;
}

#endif // Q_OS_WIN