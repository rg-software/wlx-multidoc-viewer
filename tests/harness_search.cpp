// Headless phrase-search regression harness (fix-reflow-phrase-search).
// Verifies that multi-word phrases match through MuPDF's structured text:
//   - a phrase whose words sit on one visual line (spaces must be preserved)
//   - a phrase that wraps across a line break (a separator space is inserted)
// Usage: harness-search <file> <phrase> [phrase ...]
// PASS when every phrase reports at least one hit.

#include "mupdfengine.h"

#include <QCoreApplication>
#include <cstdio>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 3) {
        std::printf("usage: harness-search <file> <phrase> [phrase ...]\n");
        return 2;
    }

    MuPdfEngine engine;
    if (!engine.open(QString::fromLocal8Bit(argv[1]))) {
        std::printf("FAIL open %s\n", argv[1]);
        return 1;
    }

    const int pages = engine.pageCount();
    if (!engine.supportsSearch() || pages <= 0) {
        std::printf("FAIL engine does not support search (pages=%d)\n", pages);
        return 1;
    }

    bool ok = true;
    for (int a = 2; a < argc; ++a) {
        const QString needle = QString::fromLocal8Bit(argv[a]);
        int hits = 0;
        for (int p = 1; p <= pages; ++p)
            hits += engine.searchText(p, needle, false).size();
        const bool found = hits > 0;
        std::printf("needle=\"%s\" hits=%d%s\n", needle.toUtf8().constData(), hits,
                    found ? "" : "  <-- NO MATCH");
        ok = ok && found;
    }

    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
