#ifndef CHMENGINE_H
#define CHMENGINE_H

#include "document.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QTemporaryFile>
#include <QVector>
#include <memory>
#include <mutex>

struct chmFile;
struct fz_context;
struct fz_document;
struct fz_page;

// CHM (Microsoft Compiled HTML Help) engine. Pages are the archive's
// .htm/.html entries ordered by reading order: home topic, then .hhc
// table-of-contents order, then remaining archive entries. Each page's bytes
// are rendered through MuPDF's HTML pipeline into a bitmap; text selection
// and search come from MuPDF's structured-text extraction over the same page.
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
    QString extractText(int page) override;
    QString metadata(const QString& key) const override;
    QVector<OutlineItem> outline() const override;
    PageInfo pageDimensions(int page) const override;

    bool supportsSearch() const override { return true; }
    QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase) override;

private:
    struct OpenedHtmlPage {
        fz_document* doc = nullptr;
        fz_page* page = nullptr;
        void drop(fz_context* ctx);
    };

    // Callers must hold m_mutex; every helper below assumes it.
    void dropArchive();
    QByteArray readEntry(const QString& path) const;
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
    int pageIndexOf(const QString& path) const;
    int pageIndexFor(const QString& path) const;
    QString decodeText(const QByteArray& bytes) const;

    chmFile* m_chm = nullptr;
    fz_context* m_fzCtx = nullptr;
    QVector<QString> m_htmlPages;
    QVector<OutlineItem> m_outline;
    mutable QHash<int, PageInfo> m_dimCache;

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
