// Headless issue-#21 repro harness (epub issues).
//
// Usage: harness-epub21 <examples-dir>
//
// Probes the three sub-reports without needing Total Commander:
//   1. links: epub21-links.epub exposes internal text<->notes links;
//      epub21-fb2notes.fb2 exposes its l:href endnote link.
//   2. strictness: epub21-bad-ncx.epub (reporter's <contentsrc=/> typo) and
//      epub21-unclosed.epub (truly unclosed tag) report open() success,
//      page count, and outline size.
//   3. stray .txt: epub21-txt.epub (extra about.txt/list.txt/
//      calibre_bookmarks.txt, unlisted in the manifest) reports open()
//      success and page count.
// PASS when the probe completes; individual expectations are informational
// [ok]/[FAIL] lines so the current (pre-fix) behavior is recorded.

#include "mupdfengine.h"
#include "viewercontroller.h"

#include <QCoreApplication>
#include <QVector>

#include <cstdio>
#include <memory>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

void dumpLinks(MuPdfEngine& engine, int pages) {
    for (int p = 1; p <= pages; ++p) {
        const QVector<LinkItem> links = engine.pageLinks(p);
        std::printf("  page %d: %d link(s)\n", p, static_cast<int>(links.size()));
        for (const LinkItem& l : links) {
            std::printf("    destPage=%d anchorY=%.3f uri=\"%s\" bbox=[%.0f,%.0f %.0fx%.0f]\n",
                        l.destPage, static_cast<double>(l.anchorY),
                        l.uri.toUtf8().constData(),
                        l.bbox.x(), l.bbox.y(), l.bbox.width(), l.bbox.height());
        }
    }
}

void probeBook(const QString& path, bool dumpAllLinks, bool requireOpen = true) {
    MuPdfEngine engine;
    const bool opened = engine.open(path);
    std::printf("OPEN %s: %s\n", qUtf8Printable(path), opened ? "yes" : "NO");
    if (!opened) {
        // Strictness probes (bad NCX) are informational until the tolerant
        // NCX open lands; everything else must open.
        if (requireOpen)
            check(false, "book opens");
        else
            std::printf("  [info] book refused (strict NCX handling)\n");
        return;
    }
    const int pages = engine.pageCount();
    std::printf("  pages=%d\n", pages);
    check(pages > 0, "page count nonzero");
    const QVector<OutlineItem> outline = engine.outline();
    std::printf("  outline top-level entries=%d\n", static_cast<int>(outline.size()));
    if (dumpAllLinks)
        dumpLinks(engine, pages);
    engine.close();
}

} // namespace

// Regression for the epub21-malformed-links report (CHAPTER ONE dead):
// followLink used to return early when goToPage(dest) reported "already
// there", so a same-page #fragment link never applied its anchor scroll.
// A tall two-page stub makes the anchor offset observable headlessly.
class StubLinkEngine : public DocumentEngine {
public:
    bool open(const QString&) override { m_open = true; return true; }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    int pageCount() const override { return 2; }
    QImage renderPage(int, float, float, int) override { return {}; }
    QString extractText(int) override { return {}; }
    QString metadata(const QString&) const override { return {}; }
    QVector<OutlineItem> outline() const override { return {}; }
    PageInfo pageDimensions(int) const override {
        PageInfo p;
        p.width = 200;
        p.height = 2000;
        return p;
    }
    QVector<LinkItem> pageLinks(int page) override {
        QVector<LinkItem> out;
        if (page == 1) {
            LinkItem self; // CHAPTER ONE shape: same page + mid-page anchor
            self.bbox = QRectF(10, 10, 50, 10);
            self.destPage = 1;
            self.anchorY = 0.5f;
            out.append(self);
            LinkItem cross; // NOTES shape: another page, page top
            cross.bbox = QRectF(10, 30, 50, 10);
            cross.destPage = 2;
            out.append(cross);
        }
        return out;
    }

private:
    bool m_open = false;
};

void controllerSamePageLinkCheck() {
    ViewerController controller;
    controller.closeDocument();
    controller.setEngine(std::make_unique<StubLinkEngine>());
    if (!controller.openDocument(QStringLiteral("stub.pdf"))) {
        check(false, "controller opens stub document");
        return;
    }
    controller.goToPage(1);
    const int scrolled = controller.followLink(1, 0, 0);
    check(scrolled > 0, "same-page anchor link scrolls to its anchor");
    const int top = controller.followLink(2, 0, scrolled);
    Q_UNUSED(top);
    controller.goToPage(1);
    const int across = controller.followLink(1, 1, 0);
    check(across != 0 || controller.currentPage() == 2,
          "cross-page link leaves page 1");
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::printf("usage: harness-epub21 <examples-dir>\n");
        return 2;
    }
    const QString dir = QString::fromLocal8Bit(argv[1]);

    std::printf("== 1. links ==\n");
    probeBook(dir + "/epub21-links.epub", true);
    probeBook(dir + "/epub21-fb2notes.fb2", true);

    std::printf("== 2. strictness ==\n");
    probeBook(dir + "/epub21-bad-ncx.epub", false, false);
    probeBook(dir + "/epub21-unclosed.epub", false, false);

    std::printf("== 3. stray txt ==\n");
    probeBook(dir + "/epub21-txt.epub", false);

    std::printf("== 4. malformed body links ==\n");
    probeBook(dir + "/epub21-malformed-links.epub", true);

    std::printf("== 5. controller same-page anchor ==\n");
    controllerSamePageLinkCheck();

    std::printf("RESULT: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
