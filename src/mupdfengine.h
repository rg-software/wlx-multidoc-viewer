#ifndef MUPDFENGINE_H
#define MUPDFENGINE_H

#include "document.h"

#include <mupdf/fitz.h>
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

    // Serializes every fz_* call: the search worker thread shares the context
    // with the UI thread's render path, so all engine calls are locked. Rendering
    // may pause momentarily while a page is being searched.
    mutable std::mutex m_mutex;

    fz_context* m_ctx = nullptr;
    fz_document* m_doc = nullptr;
    int m_pageCount = 0;
};

#endif // MUPDFENGINE_H
