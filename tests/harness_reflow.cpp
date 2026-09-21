// Headless check that reflowable documents take the theme document colors
// (theme-document-body, D2): render a page of an EPUB/FB2/MOBI/HTML under the
// dark theme and confirm the page background is the dark document background.
// Usage: harness-reflow <file> [dark|light]

#include "mupdfengine.h"
#include "viewer_settings.h"

#include <QCoreApplication>
#include <QImage>
#include <cstdio>

namespace {
uint32_t pixelAt(const QImage& img, int x, int y) {
    const QColor c = img.pixelColor(x, y);
    return (static_cast<uint32_t>(c.red()) << 16) | (static_cast<uint32_t>(c.green()) << 8) |
           static_cast<uint32_t>(c.blue());
}
int luma(uint32_t rgb) {
    const int r = static_cast<int>((rgb >> 16) & 0xFF);
    const int g = static_cast<int>((rgb >> 8) & 0xFF);
    const int b = static_cast<int>(rgb & 0xFF);
    return (77 * r + 150 * g + 29 * b) >> 8;
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::printf("usage: harness-reflow <file> [dark|light]\n");
        return 2;
    }
    const bool dark = (argc > 2) && QByteArray(argv[2]) == "dark";
    viewer_settings::setHostDark(dark);

    MuPdfEngine engine;
    if (!engine.open(QString::fromLocal8Bit(argv[1]))) {
        std::printf("FAIL open %s\n", argv[1]);
        return 1;
    }
    const QImage img = engine.renderPage(1, 1.0f, 1.0f, 0);
    if (img.isNull()) {
        std::printf("FAIL render\n");
        return 1;
    }
    // Sample the page margin (top-left) which is background for reflowable docs.
    const uint32_t corner = pixelAt(img, 1, 1);
    const int l = luma(corner);
    std::printf("corner=#%06X luma=%d (dark=%d)\n", corner, l, dark ? 1 : 0);
    const bool ok = dark ? (l < 96) : (l > 160);
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
