#ifndef CHMENGINE_H
#define CHMENGINE_H

#include "document.h"

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QTemporaryFile>
#include <QVector>
#include <memory>
#include <mutex>

struct chmFile;
struct fz_context;
struct fz_document;
struct fz_page;
struct fz_archive;

// Lazy MuPDF archive over the open CHM, so a topic opened from an in-memory
// buffer can still resolve its relative resources (e.g. <img> images). Its
// callbacks read entries straight out of the archive; defined in chmengine.cpp.
struct ChmResourceArchive;

// CHM (Microsoft Compiled HTML Help) engine. Topics are the archive's
// .htm/.html entries ordered by reading order: home topic, then .hhc
// table-of-contents order, then remaining archive entries. Each topic is laid
// out through MuPDF's HTML pipeline in the same A5 reflow box MuPdfEngine uses
// for reflowable documents, so a long topic paginates into several
// viewport-sized pages instead of one unbounded page; the public page index is
// the running page count across topics. Text selection and search come from
// MuPDF's structured-text extraction over the same pages.
class ChmEngine : public DocumentEngine {
public:
    ChmEngine() = default;
    ~ChmEngine() override;

    bool open(const QString& path) override;
    void close() override;
    bool isOpen() const override;

    int pageCount() const override;
    QImage renderPage(int page, float zoom, float dpiScale = 1.0f, int rotation = 0) override;
    PageText pageText(int page) override;
    QVector<LinkItem> pageLinks(int page) override;
    QString extractText(int page) override;
    QString metadata(const QString& key) const override;
    QVector<OutlineItem> outline() const override;
    PageInfo pageDimensions(int page) const override;

    bool supportsSearch() const override { return true; }
    QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase) override;

private:
    friend struct ChmResourceArchive;

    struct OpenedHtmlPage {
        fz_document* doc = nullptr;
        fz_page* page = nullptr;
        void drop(fz_context* ctx);
    };

    // Callers must hold m_mutex; every helper below assumes it.
    void dropArchive();
    QByteArray readEntry(const QString& path) const;
    // Whether a CHM entry exists, without reading it.
    bool entryExists(const QString& path) const;
    // Resolves a resource name MuPDF asks for (a relative URL from a topic or
    // from another resource) to an archive path: the name itself, or the name
    // resolved against the current topic's directory.
    QString resolveResourcePath(const QString& name) const;
    // Builds the lazy resource archive (m_archive) over the open CHM.
    void buildResourceArchive();
    OpenedHtmlPage openHtmlPage(int page) const;
    QImage renderPageLocked(int page, float zoom, float dpiScale, int rotation) const;
    void parseSystemData();
    void composeDocument();
    struct WindowsPaths {
        QString home;
        QString toc;
    };
    WindowsPaths windowsPaths() const;
    QVector<OutlineItem> parseWindowsOutline() const;
    QString stringAt(const QByteArray& blob, unsigned offset) const;
    // Resolves a topic path (with or without a `#fragment`) to its 0-based
    // topic index in m_htmlPages, or -1.
    int pageIndexOf(const QString& path) const;
    // First 1-based global page of the topic holding `path`, or 1 when it is
    // not a known topic.
    int firstPageForPath(const QString& path) const;
    // Maps a 1-based global page to its source topic and 0-based page within
    // that topic's A5 layout. False when `page` is out of range.
    bool topicOfPage(int page, int& topicIndex, int& pageInTopic) const;
    int firstPageOfTopic(int topic) const;
    // One heading's destination inside a topic after A5 pagination: the 0-based
    // page within the topic plus the normalized (0..1) y inside that page.
    struct AnchorTarget {
        int pageInTopic = 0;
        float anchorY = 0.0f;
    };
    // Resolves one `#fragment` (or a batch of them) within a topic, opening the
    // topic's HTML once per call. Unresolved fragments are absent from the batch
    // result; a missing single fragment yields the topic's first page.
    AnchorTarget resolveTopicFragment(int topic, const QString& fragment) const;
    QHash<QString, AnchorTarget> resolveTopicFragments(int topic, const QSet<QString>& fragments) const;
    // Opens a topic's HTML (decoded to UTF-8) and lays it out in the A5 reflow
    // box. Callers must drop the result.
    OpenedHtmlPage openTopicDoc(int topic) const;
    // Lays out every topic to count its A5 pages and builds the topic -> global
    // page map. Must run after m_htmlPages is finalized.
    void countTopicPages();
    QString decodeText(const QByteArray& bytes) const;

    chmFile* m_chm = nullptr;
    fz_context* m_fzCtx = nullptr;
    QVector<QString> m_htmlPages;
    QVector<OutlineItem> m_outline;
    mutable QHash<int, PageInfo> m_dimCache;
    // A5 reflow pagination: pages per topic (parallel to m_htmlPages), the
    // prefix sums that map a topic to its first global page, and the total.
    QVector<int> m_topicPageCount;
    QVector<int> m_topicBase;
    int m_pageCount = 0;

    // Relative-resource resolution: a lazy archive over the CHM, the cached
    // entry list it lists, and the directory of the topic currently being
    // opened (relative URLs resolve against it).
    fz_archive* m_archive = nullptr;
    QVector<QByteArray> m_archiveNames;
    mutable QString m_resourceBase;

    QString m_title;
    QString m_creator;
    QString m_systemHome;
    QString m_systemToc;
    int m_codepage = 1252;

    // libchm opens archives through the narrow ANSI API on Windows; a temp
    // copy stages files whose names are not representable in the system
    // codepage. Auto-removed when it goes out of scope.
    std::unique_ptr<QTemporaryFile> m_stagedFile;

    mutable std::mutex m_mutex;
};

#endif // CHMENGINE_H
