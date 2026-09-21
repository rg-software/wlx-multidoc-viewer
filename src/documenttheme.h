#ifndef DOCUMENT_THEME_H
#define DOCUMENT_THEME_H

// Document-body theming shared by the document engines. Reflowable formats are
// styled through an internal stylesheet (built here); fixed-layout pages are
// recolored only when they are effectively monochrome, so documents that carry
// real color are preserved. The light palette is black-on-white, which makes
// every transform below an identity — no switch is needed.

#include "viewer_settings.h"

#include <QImage>

#include <algorithm>
#include <cstdint>
#include <string>

namespace documenttheme {

// A pixel is "colored" when its channel spread exceeds this; a page is treated
// as monochrome when the colored fraction stays at or below the cutoff.
inline constexpr int kChromaThreshold = 24;
inline constexpr double kColoredFractionMax = 0.005;

// True when the image is effectively monochrome (gray text/scans/line art) and
// therefore safe to recolor with the theme's document colors.
inline bool isMonochrome(const QImage& img) {
    if (img.isNull())
        return false;
    const QImage src = (img.format() == QImage::Format_RGB888)
                           ? img
                           : img.convertToFormat(QImage::Format_RGB888);
    const int w = src.width();
    const int h = src.height();
    if (w <= 0 || h <= 0)
        return false;

    // Sample at most ~1M pixels so very large renders stay cheap.
    const int step = std::max(1, (w * h) / 1000000);
    long long colored = 0;
    long long total = 0;
    for (int y = 0; y < h; ++y) {
        const uchar* row = src.constScanLine(y);
        for (int x = 0; x < w; x += step) {
            const uchar* p = row + static_cast<size_t>(x) * 3;
            const int r = p[0], g = p[1], b = p[2];
            const int mx = std::max({r, g, b});
            const int mn = std::min({r, g, b});
            ++total;
            if (mx - mn > kChromaThreshold)
                ++colored;
        }
    }
    return total > 0 && static_cast<double>(colored) / static_cast<double>(total) <= kColoredFractionMax;
}

// Remaps each pixel's luminance between `text` (black) and `bg` (white).
// Identity when text is black and bg is white (the light palette).
inline void applyDuotone(QImage& img, uint32_t text, uint32_t bg) {
    if (img.isNull())
        return;
    if (img.format() != QImage::Format_RGB888)
        img = img.convertToFormat(QImage::Format_RGB888);
    const int tr = static_cast<int>((text >> 16) & 0xFF);
    const int tg = static_cast<int>((text >> 8) & 0xFF);
    const int tb = static_cast<int>(text & 0xFF);
    const int br = static_cast<int>((bg >> 16) & 0xFF);
    const int bgc = static_cast<int>((bg >> 8) & 0xFF);
    const int bb = static_cast<int>(bg & 0xFF);
    const int w = img.width();
    const int h = img.height();
    for (int y = 0; y < h; ++y) {
        uchar* row = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            uchar* p = row + static_cast<size_t>(x) * 3;
            const int lum = (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8; // 0..255
            p[0] = static_cast<uchar>(tr + ((br - tr) * lum) / 255);
            p[1] = static_cast<uchar>(tg + ((bgc - tg) * lum) / 255);
            p[2] = static_cast<uchar>(tb + ((bb - tb) * lum) / 255);
        }
    }
}

// Applies the active palette's document theme to a rendered page in place.
inline void applyToRenderedPage(QImage& img) {
    const viewer_settings::Palette& p = viewer_settings::activePalette();
    if (p.documentBg == 0xFFFFFFu && p.documentText == 0x000000u)
        return; // light theme: identity
    if (!isMonochrome(img))
        return;
    applyDuotone(img, p.documentText, p.documentBg);
}

// Internal stylesheet for reflowable documents, built from the palette. The
// background is set on `html` (a document's own body background covers it) and
// the text color on `body` (elements that set their own color keep it).
inline std::string reflowCss() {
    const viewer_settings::Palette& p = viewer_settings::activePalette();
    auto hex = [](uint32_t c) {
        static const char* d = "0123456789ABCDEF";
        std::string s = "#";
        for (int shift = 20; shift >= 0; shift -= 4)
            s += d[(c >> shift) & 0xF];
        return s;
    };
    return "html, body { background-color: " + hex(p.documentBg) +
           "; } body { color: " + hex(p.documentText) + "; }";
}

} // namespace documenttheme

#endif // DOCUMENT_THEME_H
