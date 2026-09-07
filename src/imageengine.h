#ifndef IMAGEENGINE_H
#define IMAGEENGINE_H

#include "document.h"

#include <QImageReader>
#include <QString>
#include <mutex>

// Standalone raster image engine for natively-decodable image files
// (jpg/jpeg/png/gif/bmp/ico),
// decoded through Qt's QImageReader so multi-frame animation metadata (per-
// frame delays, loop count) is available. Every file is a single document
// page; multi-frame GIFs animate in place via the DocumentEngine animation
// virtuals instead of being exposed as step-through pages. When QImageReader
// cannot decode a file, open() returns false so the caller falls back to the
// MuPDF engine. Image-only content: no text layer, no outline.
class ImageEngine : public DocumentEngine {
public:
    ImageEngine() = default;
    ~ImageEngine() override;

    bool open(const QString& path) override;
    void close() override;
    bool isOpen() const override;

    int pageCount() const override;
    QImage renderPage(int page, float zoom, float dpiScale = 1.0f, int rotation = 0) override;
    QString extractText(int page) override;
    QString metadata(const QString& key) const override;
    QVector<OutlineItem> outline() const override;
    PageInfo pageDimensions(int page) const override;

    // Animation (GIF). All callers hold m_mutex; helpers assume it.
    bool isAnimated() const override;
    int frameDelayMs() const override;
    bool advanceFrame() override;

private:
    // Decodes frames 0..frameIndex sequentially from a freshly-constructed
    // reader (Qt's GIF handler only advances via read(); jumpToImage is a
    // no-op for animated GIFs) and returns the last decoded frame.
    QImage readFrameImage(QImageReader& reader, int frameIndex) const;
    QImage finishImage(QImage img, float zoom, float dpiScale, int rotation) const;
    void dropImage();

    QString m_path;
    int m_frameCount = 0;
    int m_currentFrame = 0;
    int m_frameDelayMs = 0;
    int m_loopCount = 0;   // 0 / negative = loop forever
    int m_loopsDone = 0;
    int m_dimWidth = 0;
    int m_dimHeight = 0;
    bool m_valid = false;
    bool m_animated = false;

    mutable std::mutex m_mutex;
};

#endif // IMAGEENGINE_H