#ifndef DJVUENGINE_H
#define DJVUENGINE_H

#include "document.h"

#include <libdjvu/ddjvuapi.h>
#include <QHash>
#include <QString>
#include <QVector>

class DjVuEngine : public DocumentEngine {
public:
    DjVuEngine();
    ~DjVuEngine() override;

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
    bool supportsSearch() const override { return false; }
    QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase) override;

private:
    ddjvu_context_t* m_ctx = nullptr;
    ddjvu_document_t* m_doc = nullptr;
    ddjvu_format_t* m_fmt = nullptr;
    int m_pageCount = 0;
    QString m_path;

    // Measuring a page dimension is expensive in DjVuLibre (very page create +
    // synchronous decode wait), and the viewer asks for every page's dimensions
    // on each layout rebuild. Cache per-page PageInfo; cleared on close().
    mutable QHash<int, PageInfo> m_dimCache;
};

#endif // DJVUENGINE_H
