// Headless outline-destination harness (fix-epub-outline-navigation).
//
// Usage: harness-epub-toc <examples-dir>
//
// MuPDF's EPUB outline loader (source/html/epub-doc.c) stores the table-of-
// contents destination only in fz_outline::uri and leaves fz_outline::page at
// fz_make_location(-1, -1) -- unlike the PDF, XPS and HTML/MOBI loaders,
// which resolve the destination up front. The engine therefore has to resolve
// the URI itself, otherwise every EPUB TOC entry points at page 0 and the
// sidebar silently navigates to page 1.
//
// Checks (per EPUB fixture):
//   - the EPUB3 nav document and the EPUB2 NCX produce the same outline;
//   - every entry resolves to a real page inside the document (never page 0);
//   - chapter one starts on page 1, the fragment entry lands on a later page
//     of chapter one with a non-zero in-page anchor, and chapter two starts
//     after it (the page numbers themselves depend on the reflow layout);
//   - the in-page link to the same fragment resolves to the same page and
//     the same position as the table-of-contents entry;
//   - activating an entry through the real sidebar presenter navigates the
//     real controller to the destination page.
//
// The PDF outline (MuPDF resolves it into the node itself) is checked as well,
// so the EPUB URI path cannot regress the fast path.
//
// PASS when every required property holds.

#include "favorites.h"
#include "mupdfengine.h"
#include "sidebar.h"
#include "viewercontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QVector>

#include <cmath>
#include <cstdio>
#include <memory>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

// Minimal sidebar backend: the presenter only needs the rows to exist.
class FakeSidebarBackend : public SidebarBackend {
public:
    void clearEntries() override {
        m_titles.clear();
    }
    void addEntry(int id, int parentId, const QString& title) override {
        Q_UNUSED(parentId);
        m_titles.append(title);
    }
    void selectEntry(int id) override { m_selected = id; }
    void setVisible(bool on) override { m_visible = on; }

    QVector<QString> m_titles;
    int m_selected = -1;
    bool m_visible = false;
};

struct FlatEntry {
    QString title;
    int pageNo = 0;
    float anchorY = 0.0f;
    bool resolved = false;
};

void flatten(const QVector<OutlineItem>& items, QVector<FlatEntry>& out) {
    for (const OutlineItem& it : items) {
        FlatEntry e;
        e.title = it.title;
        e.pageNo = it.pageNo;
        e.anchorY = it.anchorY;
        e.resolved = it.resolved;
        out.append(e);
        flatten(it.children, out);
    }
}

int indexOfTitle(const QVector<FlatEntry>& entries, const QString& title) {
    for (int i = 0; i < entries.size(); ++i) {
        if (entries[i].title == title)
            return i;
    }
    return -1;
}

// Opens an EPUB whose table of contents is expected to resolve, and drives
// the real sidebar presenter over the real controller.
void checkEpub(const QString& path, const QString& label) {
    std::printf("== %s ==\n", qUtf8Printable(label));

    MuPdfEngine engine;
    if (!engine.open(path)) {
        check(false, "book opens");
        return;
    }
    const int pages = engine.pageCount();
    std::printf("  %s: %d pages\n", qUtf8Printable(path), pages);
    check(pages >= 3, "book paginates to at least three pages");

    QVector<FlatEntry> flat;
    flatten(engine.outline(), flat);
    for (const FlatEntry& e : flat) {
        std::printf("    \"%s\" page=%d anchorY=%.3f resolved=%d\n",
                    qUtf8Printable(e.title), e.pageNo,
                    static_cast<double>(e.anchorY), e.resolved ? 1 : 0);
    }
    check(flat.size() == 3, "outline exposes three entries");

    bool allResolved = !flat.isEmpty();
    bool allInRange = !flat.isEmpty();
    for (const FlatEntry& e : flat) {
        allResolved = allResolved && e.resolved;
        allInRange = allInRange && e.pageNo >= 1 && e.pageNo <= pages;
    }
    check(allResolved, "every entry resolved to a destination");
    check(allInRange, "every entry targets a page inside the document");

    const int ch1 = indexOfTitle(flat, QStringLiteral("Chapter One"));
    const int sec = indexOfTitle(flat, QStringLiteral("Section 1.2"));
    const int ch2 = indexOfTitle(flat, QStringLiteral("Chapter Two"));
    check(ch1 >= 0 && sec >= 0 && ch2 >= 0, "all three titles are present");
    if (ch1 < 0 || sec < 0 || ch2 < 0)
        return;

    check(flat[ch1].pageNo == 1, "Chapter One starts on page 1");
    check(flat[sec].pageNo > flat[ch1].pageNo,
          "Section 1.2 lands on a later page of chapter one");
    check(flat[ch2].pageNo > flat[sec].pageNo,
          "Chapter Two starts after chapter one");
    check(flat[sec].anchorY > 0.05f,
          "Section 1.2 carries a mid-page anchor");
    check(flat[sec].anchorY < 1.0f, "Section 1.2 anchor stays inside the page");

    // Cross-check: the in-page link to the same fragment (chapter two links
    // back to it) must resolve to the same page and the same position as the
    // table-of-contents entry.
    const QVector<LinkItem> links = engine.pageLinks(flat[ch2].pageNo);
    std::printf("    chapter two exposes %d link(s)\n", links.size());
    check(links.size() == 1, "chapter two exposes exactly one link");
    if (links.size() == 1) {
        check(links[0].destPage == flat[sec].pageNo,
              "the in-page link and the TOC entry share a target page");
        check(std::fabs(links[0].anchorY - flat[sec].anchorY) < 0.01f,
              "the in-page link and the TOC entry share a target position");
    }

    // The real consumer path: the sidebar hands the entry to the controller,
    // which must navigate to the destination page.
    ViewerController controller;
    controller.setEngine(std::make_unique<MuPdfEngine>());
    if (!controller.openDocument(path)) {
        check(false, "controller opens the book");
        return;
    }
    FakeSidebarBackend backend;
    SidebarPresenter presenter;
    presenter.attach(&controller, &backend);
    presenter.reload();
    check(backend.m_titles.size() == 3, "sidebar lists three outline rows");

    for (const FlatEntry& e : flat) {
        const int id = backend.m_titles.indexOf(e.title);
        if (id < 0) {
            check(false, "sidebar row present for the outline entry");
            continue;
        }
        presenter.onEntryActivated(id);
        check(controller.currentPage() == e.pageNo,
              "activating the entry navigates to its destination page");
    }
}

// The PDF outline is resolved by MuPDF itself (the destination lands in
// fz_outline::page), so this guards the fast path against a regression from
// the EPUB URI path.
void checkPdf(const QString& path) {
    std::printf("== PDF fast path ==\n");

    MuPdfEngine engine;
    if (!engine.open(path)) {
        check(false, "pdf opens");
        return;
    }
    const int pages = engine.pageCount();
    QVector<FlatEntry> flat;
    flatten(engine.outline(), flat);
    for (const FlatEntry& e : flat) {
        std::printf("    \"%s\" page=%d anchorY=%.3f resolved=%d\n",
                    qUtf8Printable(e.title), e.pageNo,
                    static_cast<double>(e.anchorY), e.resolved ? 1 : 0);
    }
    check(flat.size() == 2, "pdf outline exposes two entries");
    if (flat.size() != 2)
        return;

    bool allResolved = true;
    bool allInRange = true;
    for (const FlatEntry& e : flat) {
        allResolved = allResolved && e.resolved;
        allInRange = allInRange && e.pageNo >= 1 && e.pageNo <= pages;
    }
    check(allResolved, "every pdf entry resolved to a destination");
    check(allInRange, "every pdf entry targets a page inside the document");
    check(flat[0].pageNo == 1, "pdf entry 1 targets page 1");
    check(flat[1].pageNo == 2, "pdf entry 2 targets page 2");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::printf("usage: harness-epub-toc <examples-dir>\n");
        return 2;
    }
    const QString dir = QString::fromLocal8Bit(argv[1]);

    // Keep the real favorites store out of the way: the sidebar appends a
    // Favorites section when the open document has marks, which would change
    // the row ids the activation checks rely on.
    FavoritesStore::setLocationOverrideForTesting(
        QDir::temp().filePath(QStringLiteral("wlx-harness-epub-toc")));

    checkEpub(dir + "/epub-toc-nav3.epub", "EPUB3 nav document");
    checkEpub(dir + "/epub-toc-ncx2.epub", "EPUB2 NCX");
    checkPdf(dir + "/sample-links.pdf");

    std::printf("RESULT: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
