#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QString>
#include <QImage>
#include <QRectF>
#include <QVector>
#include <memory>

struct OutlineItem {
    QString title;
    int pageNo = 0;
    QVector<OutlineItem> children;
};

struct PageInfo {
    int width = 0;
    int height = 0;
};

// A word of selectable text with its geometry. bbox is in page space (y-down,
// page dimensions from pageDimensions(), unrotated, pre-zoom). lineIndex groups
// words into visual lines for text assembly and highlight joins.
struct TextWord {
    QString text;
    QRectF bbox;
    int lineIndex = 0;
};

// Per-page selectable text layer. hasText is false for pages without a
// machine-readable text layer (scans, photo archives).
struct PageText {
    bool hasText = false;
    QVector<TextWord> words;
};

class DocumentEngine {
public:
    virtual ~DocumentEngine() = default;

    virtual bool open(const QString& path) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual int pageCount() const = 0;
    virtual QImage renderPage(int page, float zoom, float dpiScale = 1.0f, int rotation = 0) = 0;
    virtual QString extractText(int page) = 0;
    virtual QString metadata(const QString& key) const = 0;
    virtual QVector<OutlineItem> outline() const = 0;
    virtual PageInfo pageDimensions(int page) const = 0;

    // Geometry-aware selectable-text API. Default implementations report no
    // text so engines that cannot expose a text layer (image formats, CHM
    // without extraction, etc.) are correct without override.
    virtual bool hasSelectableText(int page) { return !pageText(page).words.isEmpty(); }
    virtual PageText pageText(int page) { return {}; }
};

std::unique_ptr<DocumentEngine> createEngine(const QString& path);

#endif // DOCUMENT_H
