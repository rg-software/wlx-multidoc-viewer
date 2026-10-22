#ifndef MUPDFENGINE_H
#define MUPDFENGINE_H

#include "document.h"
#include "mupdfcontext.h"

#include <mupdf/fitz.h>
#include <QImage>
#include <QString>

// Page box in points (A5) used to lay out reflowable documents (EPUB/MOBI/HTML/
// FB2), matching muPDF's built-in default. The font size inside the box comes
// from [Viewer] FontSize (viewer_settings::kReflowFontSize). Shared with
// tests/harness_epub_count.cpp so its layout reference stays in sync.
inline constexpr float kReflowPageWidthPt = 420.0f;
inline constexpr float kReflowPageHeightPt = 595.0f;

class MuPdfEngine : public DocumentEngine {
public:
    MuPdfEngine();
    ~MuPdfEngine() override;

    bool open(const QString& path) override;

    // Opens an in-memory document image (e.g. the FB2 entry extracted from a
    // .fb2.zip). magic is a virtual filename whose extension selects the
    // MuPDF handler, so callers must pass a name ending in the real content
    // type ("book.fb2", never the outer container name "book.fb2.zip").
    bool openBuffer(const QByteArray& data, const QString& magic);

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
    // Drops the open document; the shared context is never dropped (it is
    // process-lifetime). Caller must hold the shared MuPDF lock.
    void dropDocument();

    // Attaches m_ctx to the shared process-wide context. source is used only
    // for diagnostics. Caller must hold the shared MuPDF lock with m_ctx == nullptr.
    bool makeContext(const QString& source);

    // Shared post-open tail: reflow layout, theming, and the final page
    // count. suffix is the lower-cased extension of the source name (drives
    // the FB2 duotone-vs-stylesheet theming split); source is diagnostic only.
    // Caller must hold the shared MuPDF lock with m_doc already opened.
    bool finishOpen(const QString& source, const QString& suffix);

    // Parses a MOBI/PRC stream and, if an EXTH cover record is present and
    // decodable, stores it into m_coverImage and returns true. Does NOT open
    // or modify m_doc; open() always uses the plain MuPDF open so section
    // page-breaks in the body are preserved byte-for-byte. On failure m_cover
    // state is left untouched and false is returned (fallback = no cover).
    // Caller must hold the shared MuPDF lock.
    bool tryOpenMobi(fz_stream* file);

    // Serializes every fz_* call process-wide via sharedMuPdfMutex(): the
    // search worker thread shares the context with the UI thread's render
    // path, and now also with every other open engine instance, so all engine
    // calls take the global lock. Rendering may pause momentarily while a
    // page is being searched.

    fz_context* m_ctx = nullptr;
    fz_document* m_doc = nullptr;
    int m_pageCount = 0;

    // True for reflowable documents (EPUB/MOBI/FB2/HTML): their bodies are
    // themed through the user stylesheet, not the per-page duotone.
    bool m_isReflowable = false;

    // Some reflowable formats paint an opaque page background that the user
    // stylesheet cannot override (MuPDF's FB2 handler does); those are themed
    // with the per-page duotone instead, like fixed-layout pages.
    bool m_themePagesByDuotone = false;

    // Synthetic cover page: when true, public page 1 is m_coverImage and all
    // body pages shift by +1 (body page n is MuPDF page n-1). Set while the
    // caller holds the shared MuPDF lock.
    bool m_bodyHasCover = false;
    QImage m_coverImage;

    // Maps a public page number to the MuPDF page index (0-based) of the body.
    // Returns <0 for the synthetic cover page. Caller must hold the shared
    // MuPDF lock (sharedMuPdfMutex()).
    int bodyPageIndex(int page) const {
        if (m_bodyHasCover)
            return page - 2;   // public 1 -> cover (-1), public 2 -> body 0
        return page - 1;       // public 1 -> body 0
    }

    // Unshifted page dimensions of body page (1-based body number); used by the
    // cover renderer to size page 1. Caller must hold the shared MuPDF lock.
    PageInfo pageDimensionsRaw(int bodyPage) const;

    // Resolves an outline entry's destination to a public page number and an
    // optional normalized in-page anchor. Returns false when the destination
    // cannot be resolved (a pure container or a dangling link).
    // Caller must hold the shared MuPDF lock.
    bool resolveOutlineTarget(const fz_outline* node, int& pageNo,
                               float& anchorY) const;
};

#endif // MUPDFENGINE_H
