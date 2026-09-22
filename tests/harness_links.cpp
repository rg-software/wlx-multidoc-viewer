// Headless hyperlink-extraction harness (add-hyperlink-navigation).
//
// Usage: harness-links <linked.pdf> [linked.chm]
//
// Verifies that the engines expose a page's hyperlinks through
// DocumentEngine::pageLinks:
//   - the PDF fixture exposes an internal page link, an external URI link, and
//     an internal link carrying an XYZ anchor coordinate;
//   - the CHM fixture exposes an internal topic link (relative href), an
//     external link, and a #fragment link resolved to the target topic.
// PASS when every required property holds.

#include "chmengine.h"
#include "mupdfengine.h"

#include <QCoreApplication>
#include <algorithm>
#include <cstdio>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::printf("usage: harness-links <linked.pdf> [linked.chm]\n");
        return 2;
    }

    {
        MuPdfEngine engine;
        const QString path = QString::fromLocal8Bit(argv[1]);
        if (!engine.open(path)) {
            std::printf("FAIL open %s\n", argv[1]);
            return 1;
        }
        const int pages = engine.pageCount();
        std::printf("PDF %s: %d pages\n", argv[1], pages);
        check(pages >= 2, "PDF has at least two pages");

        bool internalTo2 = false;
        bool externalHttp = false;
        bool anchored = false;
        float maxAnchor = 0.0f;
        const QVector<LinkItem> links = engine.pageLinks(1);
        std::printf("  page 1: %d link(s)\n", static_cast<int>(links.size()));
        for (const LinkItem& l : links) {
            std::printf("    destPage=%d anchorY=%.3f uri=\"%s\"\n",
                        l.destPage, static_cast<double>(l.anchorY),
                        l.uri.toUtf8().constData());
            if (l.destPage == 2)
                internalTo2 = true;
            if (l.uri.startsWith(QLatin1String("http")))
                externalHttp = true;
            maxAnchor = std::max(maxAnchor, l.anchorY);
            if (l.destPage >= 1 && l.anchorY > 0.01f)
                anchored = true;
        }
        check(internalTo2, "page 1 has an internal link to page 2");
        check(externalHttp, "page 1 has an external http(s) link");
        check(anchored, "page 1 has an internal link with a non-zero anchor");
        check(!engine.pageLinks(0).size(), "out-of-range page reports no links");
    }

    if (argc >= 3) {
        ChmEngine engine;
        const QString path = QString::fromLocal8Bit(argv[2]);
        if (!engine.open(path)) {
            std::printf("FAIL open %s\n", argv[2]);
            return 1;
        }
        const int pages = engine.pageCount();
        std::printf("CHM %s: %d pages\n", argv[2], pages);
        check(pages >= 2, "CHM has at least two topics");

        bool internalTopic = false;
        bool fragmentToTopic = false;
        bool external = false;
        bool anchored = false;
        for (int p = 1; p <= pages; ++p) {
            for (const LinkItem& l : engine.pageLinks(p)) {
                std::printf("  p%d: destPage=%d anchorY=%.3f uri=\"%s\"\n",
                            p, l.destPage, static_cast<double>(l.anchorY),
                            l.uri.toUtf8().constData());
                if (l.destPage >= 1 && l.destPage <= pages)
                    internalTopic = true;
                if (l.destPage == 2)
                    fragmentToTopic = true;
                if (!l.uri.isEmpty())
                    external = true;
                if (l.destPage >= 1 && l.anchorY > 0.01f)
                    anchored = true;
            }
        }
        check(internalTopic, "CHM exposes an internal topic link");
        check(fragmentToTopic, "CHM relative link resolves to topic 2");
        check(external, "CHM exposes an external link");
        check(anchored, "CHM #fragment link resolves to a non-zero anchor");
    }

    std::printf("RESULT: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
