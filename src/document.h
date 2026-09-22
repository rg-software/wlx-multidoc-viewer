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
    // False when the item carries no resolvable destination (pure container
    // or dangling link); engines fall back to page 1 on activation, while
    // reading-position sync must skip these entries.
    bool resolved = true;
    // Normalized (0..1) vertical position of the heading's destination within
    // pageNo's page, matching LinkItem::anchorY; 0 = page top / no anchor.
    // Lets a TOC entry target a fragment that lands mid-page (reflowable HTML
    // paginated into A5 pages) instead of only the page top.
    float anchorY = 0.0f;
    QVector<OutlineItem> children;
};

struct PageInfo {
    int width = 0;
    int height = 0;
};

// One page's worth of text-search hits. page is 1-based; rects are normalized
// (0..1 in each axis relative to the page size from pageDimensions(), y-down,
// unrotated, pre-zoom), so highlight overlays survive zoom/rotation changes and
// stay anchored to the underlying glyphs.
struct TextMatch {
    int page = 0;
    QVector<QRectF> rects;
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

// A hyperlink hot zone on a page. bbox is in page space (y-down, page
// dimensions from pageDimensions(), unrotated, pre-zoom) — the same convention
// as TextWord.bbox. Exactly one destination form is used:
//   - internal: destPage is the 1-based public page number; anchorY is the
//     normalized (0..1) vertical position of the destination within that page,
//     or 0 for the page top.
//   - external: destPage == 0 and uri carries the raw target (e.g. http/https/
//     mailto); the viewer hands it to the operating system.
struct LinkItem {
    QRectF bbox;
    int destPage = 0;
    float anchorY = 0.0f;
    QString uri;
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

    // Per-page hyperlink hot zones. Default reports no links so engines without
    // link support (comics, standalone images, DjVu without a text layer) are
    // correct without override. bbox uses the same page-space convention as
    // TextWord; see LinkItem for the destination forms.
    virtual QVector<LinkItem> pageLinks(int page)
    {
        Q_UNUSED(page)
        return {};
    }

    // Optional in-place animation (currently GIF). Defaults implement a static
    // single-frame document so engines that do not animate (PDF, comics, ...)
    // are correct without override. frameDelayMs returns the delay of the frame
    // about to show; advanceFrame advances playback and returns whether it
    // should continue (false when the declared loop count is exhausted).
    virtual bool isAnimated() const { return false; }
    virtual int frameDelayMs() const { return 0; }
    virtual bool advanceFrame() { return false; }

    // Positional whole-document text search. Rects in returned TextMatch are
    // normalized page space (see TextMatch). Default implementations report no
    // capability, so engines that cannot expose a positional text layer are
    // correct without override (the viewer then disables find controls).
    virtual bool supportsSearch() const { return false; }
    virtual QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase)
    {
        Q_UNUSED(page)
        Q_UNUSED(needle)
        Q_UNUSED(matchCase)
        return {};
    }
};

std::unique_ptr<DocumentEngine> createEngine(const QString& path);

#endif // DOCUMENT_H
