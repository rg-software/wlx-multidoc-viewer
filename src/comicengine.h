#ifndef COMICENGINE_H
#define COMICENGINE_H

#include "document.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>
#include <mutex>

struct archive;

// Comic book archive engine for RAR-based (CBR) and 7-Zip-based (CB7)
// containers, read through libarchive. Pages are the image entries inside
// the archive in natural order; each page is decoded through Qt's image
// loader into a bitmap. Image-only content: no text layer, no outline.
class ComicEngine : public DocumentEngine {
public:
    ComicEngine() = default;
    ~ComicEngine() override;

    bool open(const QString& path) override;
    void close() override;
    bool isOpen() const override;

    int pageCount() const override;
    QImage renderPage(int page, float zoom, float dpiScale = 1.0f, int rotation = 0) override;
    QString extractText(int page) override;
    QString metadata(const QString& key) const override;
    QVector<OutlineItem> outline() const override;
    PageInfo pageDimensions(int page) const override;

private:
    // Callers must hold m_mutex; helpers below assume it.
    void dropArchive();
    QByteArray extractEntry(const QString& name) const;
    QImage renderPageLocked(int page, float zoom, float dpiScale, int rotation) const;

    bool m_valid = false;
    QVector<QString> m_pages;
    mutable QHash<int, PageInfo> m_dimCache;
    QString m_path;

    mutable std::mutex m_mutex;
};

#endif // COMICENGINE_H
