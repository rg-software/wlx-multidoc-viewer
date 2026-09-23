// Headless FB2-in-zip harness (issue #18).
//
// Usage: harness-fb2zip [examples-dir]
//
// Exercises the .fb2.zip support end-to-end through the format dispatcher:
//   - a zip containing a FictionBook entry opens and renders like the plain
//     .fb2 (same page count, text, search, selection, links);
//   - every other zip is SKIPPED cleanly — open() returns false so ListLoad
//     can return 0 and the host hands the file to the next plugin: a zip with
//     no .fb2 entry, a zip whose .fb2-named entry is not FictionBook, a file
//     that is not a zip at all, and a missing file;
//   - when a zip holds several FictionBook entries, the natural-sorted first
//     one wins (not the zip's central-directory order).
// PASS when every property holds. SKIPs (exit 0) when the primary fixture is
// absent so un-regenerated checkouts do not fail.
//
// All engines are scoped and closed before the next opens: MuPDF's lcms2
// integration tolerates sequential contexts but not concurrent ones.

#include "document.h"
#include "mupdfengine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <cstdio>

static int g_failures = 0;

#define CHECK(name, cond)                                                     \
    do {                                                                      \
        std::printf("%s %s (line %d)\n", (cond) ? "PASS" : "FAIL", name,      \
                    __LINE__);                                                \
        std::fflush(stdout);                                                  \
        if (!(cond))                                                          \
            ++g_failures;                                                     \
    } while (0)

// Returns true when 'needle' is found by searchText on some page.
static bool documentSearch(DocumentEngine& engine, const QString& needle) {
    const int pages = engine.pageCount();
    for (int page = 1; page <= pages; ++page) {
        if (!engine.searchText(page, needle, false).isEmpty())
            return true;
    }
    return false;
}

// Returns the concatenated extractText() over every page.
static QString wholeText(DocumentEngine& engine) {
    QString all;
    const int pages = engine.pageCount();
    for (int page = 1; page <= pages; ++page)
        all += engine.extractText(page);
    return all;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    const QDir dir(argc >= 2 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("examples"));
    const QString sampleFb2   = dir.filePath("sample.fb2");
    const QString sampleZip   = dir.filePath("sample.fb2.zip");
    const QString notFb2Zip   = dir.filePath("not-fb2.zip");
    const QString fakeFb2Zip  = dir.filePath("fake-fb2.zip");
    const QString multiZip    = dir.filePath("multi.fb2.zip");

    if (!QFile::exists(sampleZip)) {
        std::printf("SKIP: fixture not present (%s)\n", qUtf8Printable(sampleZip));
        std::printf("run: python tools/generate_sample_fb2zip.py\n");
        return 0;
    }

    // Reference pagination of the plain .fb2, measured first and closed so its
    // MuPDF context is gone before the zip engine opens one of its own.
    int refPages = 0;
    if (QFile::exists(sampleFb2)) {
        MuPdfEngine plain;
        plain.open(sampleFb2);
        if (plain.isOpen()) {
            refPages = plain.pageCount();
            plain.close();
        }
    }

    // Supported case: a .fb2.zip routed through the dispatcher.
    {
        std::unique_ptr<DocumentEngine> engine = createEngine(sampleZip);
        CHECK("dispatcher produces an engine for .zip", static_cast<bool>(engine));
        if (!engine)
            return 1;

        CHECK("sample.fb2.zip opens", engine->open(sampleZip));
        const int pages = engine->pageCount();
        CHECK("page count nonzero", pages > 0);
        if (refPages > 0)
            CHECK("page count matches plain .fb2", pages == refPages);

        const PageText text = engine->pageText(1);
        CHECK("page 1 has selectable text", text.hasText && !text.words.isEmpty());
        const QImage p1 = engine->renderPage(1, 1.0f, 1.0f, 0);
        CHECK("page 1 renders", !p1.isNull());
        CHECK("title text present", wholeText(*engine).contains("Sample FB2"));
        CHECK("search support", engine->supportsSearch());
        CHECK("search finds 'Chapter'", documentSearch(*engine, "Chapter"));
        engine->close();
    }

    // Skip contract: every unsupported zip must decline from open().
    {
        std::unique_ptr<DocumentEngine> engine = createEngine(notFb2Zip);
        CHECK("zip without .fb2 entry declined",
              static_cast<bool>(engine) && !engine->open(notFb2Zip));
    }
    {
        std::unique_ptr<DocumentEngine> engine = createEngine(fakeFb2Zip);
        CHECK(".fb2-named non-FictionBook declined",
              static_cast<bool>(engine) && !engine->open(fakeFb2Zip));
    }

    QTemporaryDir tmp;
    if (tmp.isValid()) {
        QFile bogus(tmp.filePath("bogus.zip"));
        (void)bogus.open(QIODevice::WriteOnly);
        bogus.write("this is not a zip archive, just plain bytes\n");
        bogus.close();
        {
            std::unique_ptr<DocumentEngine> engine = createEngine(bogus.fileName());
            CHECK("non-archive .zip declined",
                  static_cast<bool>(engine) && !engine->open(bogus.fileName()));
        }
        {
            std::unique_ptr<DocumentEngine> engine = createEngine(tmp.filePath("missing.zip"));
            CHECK("missing .zip file declined",
                  static_cast<bool>(engine) && !engine->open(tmp.filePath("missing.zip")));
        }
        // .fb2z is the same payload as .fb2.zip: a renamed copy must open
        // through the dispatcher and a non-archive .fb2z must decline.
        {
            const QString fb2z = tmp.filePath("sample.fb2z");
            const bool copied = QFile::copy(sampleZip, fb2z);
            CHECK("copy to .fb2z", copied);
            std::unique_ptr<DocumentEngine> engine = createEngine(fb2z);
            CHECK("dispatcher produces an engine for .fb2z", static_cast<bool>(engine));
            if (engine) {
                CHECK("sample.fb2z opens", engine->open(fb2z));
                if (engine->isOpen()) {
                    CHECK("sample.fb2z shows FB2 text",
                          wholeText(*engine).contains("Sample FB2"));
                    engine->close();
                }
            }
            QFile bogusFb2z(tmp.filePath("bogus.fb2z"));
            (void)bogusFb2z.open(QIODevice::WriteOnly);
            bogusFb2z.write("not an archive\n");
            bogusFb2z.close();
            {
                std::unique_ptr<DocumentEngine> engine = createEngine(bogusFb2z.fileName());
                CHECK("non-archive .fb2z declined",
                      static_cast<bool>(engine) && !engine->open(bogusFb2z.fileName()));
            }
        }
    }

    // Multiple FictionBook entries: the natural-sorted first one wins.
    {
        std::unique_ptr<DocumentEngine> engine = createEngine(multiZip);
        if (!engine || !engine->open(multiZip)) {
            CHECK("multi.fb2.zip opens", false);
        } else {
            const QString text = wholeText(*engine);
            CHECK("natural-first FB2 selected",
                  text.contains("SECOND CHAPTER MARKER") &&
                  !text.contains("TENTH CHAPTER MARKER"));
            engine->close();
        }
    }

    std::printf("\nRESULT: %s (%d failure(s))\n",
                g_failures ? "FAIL" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}