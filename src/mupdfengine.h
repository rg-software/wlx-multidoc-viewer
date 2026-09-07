#ifndef MUPDFENGINE_H
#define MUPDFENGINE_H

#include "document.h"

#include <mupdf/fitz.h>
#include <QImage>
#include <QString>
#include <mutex>

class MuPdfEngine : public DocumentEngine {
public:
    MuPdfEngine();
    ~MuPdfEngine() override;

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
    // Drops all MuPDF state; caller must hold m_mutex.
    void dropDocument();

    // Parses a MOBI/PRC stream and, if an EXTH cover record is present and
    // decodable, stores it into m_coverImage and returns true. Does NOT open
    // or modify m_doc; open() always uses the plain MuPDF open so section
    // page-breaks in the body are preserved byte-for-byte. On failure m_cover
    // state is left untouched and false is returned (fallback = no cover).
    // Caller must hold m_mutex.
    bool tryOpenMobi(fz_stream* file);

    // Serializes every fz_* call: the search worker thread shares the context
    // with the UI thread's render path, so all engine calls are locked. Rendering
    // may pause momentarily while a page is being searched.
    mutable std::mutex m_mutex;

    fz_context* m_ctx = nullptr;
    fz_document* m_doc = nullptr;
    int m_pageCount = 0;

    // Synthetic cover page: when true, public page 1 is m_coverImage and all
    // body pages shift by +1 (body page n is MuPDF page n-1). Set while the
    // caller holds m_mutex.
    bool m_bodyHasCover = false;
    QImage m_coverImage;

    // Maps a public page number to the MuPDF page index (0-based) of the body.
    // Returns <0 for the synthetic cover page. Caller must hold m_mutex.
    int bodyPageIndex(int page) const {
        if (m_bodyHasCover)
            return page - 2;   // public 1 -> cover (-1), public 2 -> body 0
        return page - 1;       // public 1 -> body 0
    }

    // Unshifted page dimensions of body page (1-based body number); used by the
    // cover renderer to size page 1. Caller must hold m_mutex.
    PageInfo pageDimensionsRaw(int bodyPage) const;
};

#endif // MUPDFENGINE_H
