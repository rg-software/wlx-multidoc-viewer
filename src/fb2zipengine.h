#ifndef FB2ZIPENGINE_H
#define FB2ZIPENGINE_H

#include "document.h"
#include "mupdfengine.h"

#include <QString>
#include <mutex>

// FB2-in-zip engine (issue #18): opens ".fb2.zip" containers by extracting
// the first FictionBook entry inside and rendering it through MuPdfEngine's
// FB2 handler (same layout/theming/outline/text/link path as a plain .fb2).
//
// Every other zip declares itself unsupported: open() returns false (no FB2
// entry, decryption failure, oversized entry, or content that is not
// FictionBook), which propagates up to ListLoad returning 0 so the host hands
// the file to the next lister plugin. The engine never claims zips this
// plugin does not support — in particular it never falls through to MuPDF
// directly, whose CBZ handler also registers the "zip" extension and would
// silently open any zip as a comic (or empty comic) instead of declining.
class Fb2ZipEngine : public DocumentEngine {
public:
    Fb2ZipEngine() = default;
    ~Fb2ZipEngine() override;

    bool open(const QString& path) override;
    void close() override;
    bool isOpen() const override;

    int pageCount() const override;
    QImage renderPage(int page, float zoom, float dpiScale = 1.0f, int rotation = 0) override;
    QString extractText(int page) override;
    QString metadata(const QString& key) const override;
    QVector<OutlineItem> outline() const override;
    PageInfo pageDimensions(int page) const override;

    PageText pageText(int page) override;
    QVector<LinkItem> pageLinks(int page) override;

    bool supportsSearch() const override;
    QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase) override;

private:
    // Delegate engine rendering the extracted FictionBook document.
    MuPdfEngine m_fb2;

    bool m_valid = false;
    mutable std::mutex m_mutex;
};

#endif // FB2ZIPENGINE_H