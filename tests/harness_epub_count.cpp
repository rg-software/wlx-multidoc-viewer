// Headless regression for the stale page-count bug on reflowable documents
// (EPUB/MOBI/HTML).
//
// MuPdfEngine::open used to count pages with MuPDF's *default* (publisher)
// stylesheet and only afterwards apply the theme stylesheet via
// fz_style_document(publisher_css=0, css). That call switches the document off
// the publisher CSS and invalidates the layout, so the first render re-flowed
// the html and produced a different pagination. The cached count then described
// the pre-style layout while rendering and the outline used the re-styled one,
// so navigation stopped at a non-final page (e.g. "239 pages" where page 239 is
// ~58% into the book, while the re-styled layout had 412 pages).
//
// The fix counts AFTER styling. This harness pins the invariant: the engine's
// pageCount() must match the page count of the *final* (post-style) layout.
// That reference is measured by re-running this executable with --raw (a
// separate process, because muPDF's lcms2 integration allows only one context
// per process and would otherwise refuse the engine's context).
//
// Usage: harness-epub-count <file.epub|file.mobi>

#include "documenttheme.h"
#include "mupdfengine.h"

#include <mupdf/fitz.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QString>

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

#define CHECK(name, cond)                                                     \
    do {                                                                      \
        std::printf("%s %s (line %d)\n", (cond) ? "PASS" : "FAIL", name,      \
                    __LINE__);                                                \
        std::fflush(stdout);                                                  \
        if (!(cond))                                                          \
            ++g_failures;                                                     \
    } while (0)

// --raw worker: replay MuPdfEngine::open's muPDF sequence and print the page
// count before and after the theme stylesheet is applied. Exits via _Exit so
// muPDF's teardown (which trips an access violation on some reflowable docs in
// 1.28.3) is skipped; the leaked context dies with this process.
static int runRawWorker(const QString& path) {
    const QByteArray magic = path.toUtf8();
    const std::string css = documenttheme::reflowCss();
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!ctx) {
        std::printf("raw-error=no-context\n");
        std::fflush(stdout);
        std::_Exit(2);
    }
    fz_register_document_handlers(ctx);
    fz_stream* stm = nullptr;
    fz_document* doc = nullptr;
    int pre = -1, post = -1;
    fz_try(ctx) {
#ifdef _WIN32
        stm = fz_open_file_w(ctx, reinterpret_cast<const wchar_t*>(path.utf16()));
#else
        stm = fz_open_file(ctx, path.toUtf8().constData());
#endif
        doc = fz_open_document_with_stream(ctx, magic.constData(), stm);
        pre = fz_count_pages(ctx, doc);
        fz_style_document(ctx, doc, 0, css.c_str());
        post = fz_count_pages(ctx, doc);
        std::printf("pre=%d post=%d\n", pre, post);
        std::fflush(stdout);
    }
    fz_catch(ctx) {
        std::printf("raw-error=%s\n", fz_caught_message(ctx));
        std::fflush(stdout);
        std::_Exit(2);
    }
    std::_Exit(0);
}

// Runs the --raw worker in a child process and returns its pre/post counts.
static bool measureRaw(const QString& path, int* pre, int* post) {
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {"--raw", path});
    if (!child.waitForStarted(10000))
        return false;
    if (!child.waitForFinished(120000))
        return false;
    const QString out = QString::fromLocal8Bit(child.readAllStandardOutput());
    const int pi = out.indexOf("pre=");
    const int po = out.indexOf(" post=");
    if (pi < 0 || po < 0)
        return false;
    bool ok1 = false, ok2 = false;
    *pre = out.mid(pi + 4, po - (pi + 4)).toInt(&ok1);
    *post = out.mid(po + 6).trimmed().toInt(&ok2);
    return ok1 && ok2;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc >= 3 && QByteArray(argv[1]) == "--raw")
        return runRawWorker(QString::fromLocal8Bit(argv[2]));
    if (argc < 2) {
        std::printf("usage: harness-epub-count <file.epub|file.mobi>\n");
        return 2;
    }
    const QString path = QString::fromLocal8Bit(argv[1]);
    if (!QFile::exists(path)) {
        std::printf("SKIP: fixture not present (%s)\n", qUtf8Printable(path));
        return 0;
    }

    int rawPre = -1, rawPost = -1;
    const bool haveRaw = measureRaw(path, &rawPre, &rawPost);

    MuPdfEngine e;
    CHECK("open", e.open(path));
    if (!e.isOpen()) {
        std::printf("RESULT: FAIL (%d failure(s))\n", g_failures);
        return 1;
    }
    const int count = e.pageCount();

    if (haveRaw) {
        std::printf("  [info] raw pre-style=%d post-style=%d engine=%d\n",
                    rawPre, rawPost, count);
        if (rawPre == rawPost)
            std::printf("  [note] fixture does not exercise the reflow-count "
                        "bug (pre-style == post-style); pick another file\n");
        // Regression assertion: the engine's count must describe the final,
        // post-style layout. The engine adds one synthetic page for a MOBI
        // cover, so post-style or post-style+1 are both valid.
        CHECK("count matches post-style layout",
              count == rawPost || count == rawPost + 1);
    } else {
        std::printf("  [note] could not measure raw reference; "
                    "checking only rendering\n");
    }

    const QImage p1 = e.renderPage(1, 1.0f, 1.0f, 0);
    CHECK("page 1 renders", !p1.isNull());

    CHECK("page count nonzero", count > 0);
    if (count > 0) {
        const QString lastText = e.extractText(count);
        CHECK("final page carries text", !lastText.isEmpty());
    }

    e.close();
    std::printf("\n%s (%d failure(s))\n",
                g_failures ? "RESULT: FAIL" : "RESULT: ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
