// Windows headless ICC color-management smoke harness
// (openspec change add-icc-color-management). Renders page 1 of
// examples/AC3_GW_Notebook_GER.pdf through the real MuPdfEngine and asserts
// the embedded CMYK title image converts with its ICC profile: sampled pixels
// must be dark brown (R>G>B, G and B clearly above zero), not the near-black
// red-collapsed fallback the build produced with FZ_ENABLE_ICC=0.
//
// Requires the (large, untracked) sample PDF to be present; skips (exit 0
// with SKIP line) when it is missing so the target stays CI-friendly.
// Prints PASS/FAIL lines; exits nonzero on any failure.

#include "mupdfengine.h"

#include <QImage>
#include <QString>
#include <QFile>

#include <cstdio>
#include <cstdlib>
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

int main() {
    const QString sample = QStringLiteral(
        "C:/Projects-Git/wlx-multidoc-viewer/examples/AC3_GW_Notebook_GER.pdf");
    if (!QFile::exists(sample)) {
        std::printf("SKIP: sample PDF not present\n");
        return 0;
    }

    MuPdfEngine e;
    CHECK("open AC3_GW_Notebook_GER.pdf", e.open(sample));
    if (!e.isOpen()) {
        std::printf("RESULT: FAIL (%d failure(s))\n", g_failures);
        return g_failures ? 1 : 0;
    }

    QImage p1 = e.renderPage(1, 0.25f, 1.0f, 0);
    CHECK("page 1 renders", !p1.isNull());
    if (p1.isNull()) {
        std::printf("RESULT: FAIL\n");
        return 1;
    }

    if (p1.format() != QImage::Format_RGB888)
        p1 = p1.convertToFormat(QImage::Format_RGB888);
    std::printf("  [dbg] page1 render %dx%d\n", p1.width(), p1.height());

    // Sample every 16th pixel; accumulate counts of (a) brown-ish dark pixels
    // (R>G>B with G,R>= ... ) and (b) the pathological red-collapse signature
    // (G==0 && B==0 && R>8) that the no-ICC fallback produced.
    long brown = 0, redCollapse = 0, total = 0;
    for (int y = 0; y < p1.height(); y += 16) {
        const uchar* line = p1.constScanLine(y);
        for (int x = 0; x < p1.width(); x += 16) {
            const uchar* px = line + x * 3;
            const int r = px[0], g = px[1], b = px[2];
            ++total;
            if (g == 0 && b == 0 && r > 8)
                ++redCollapse;
            if (r > g && g > b && r >= 12 && g >= 6 && b >= 4)
                ++brown;
        }
    }
    std::printf("  [dbg] sampled=%ld brown=%ld redCollapse=%ld\n",
                total, brown, redCollapse);

    // The title image is dominated by dark brown: a healthy share of samples
    // must be brown-ish. The no-ICC build showed mostly (0,0,0) plus (16,0,0):
    // redCollapse would be ~thousands and brown near zero.
    CHECK("brown pixels appear (ICC color)", brown > total / 20);
    CHECK("no red-collapse fallback signature",
          redCollapse * 4 < total);

    e.close();
    std::printf("\n%s (%d failure(s))\n",
                g_failures ? "RESULT: FAIL" : "RESULT: ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}