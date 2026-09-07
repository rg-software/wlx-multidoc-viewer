#include "imageengine.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTransform>
#include <QDebug>

ImageEngine::~ImageEngine() {
    close();
}

void ImageEngine::dropImage() {
    m_path.clear();
    m_frameCount = 0;
    m_currentFrame = 0;
    m_frameDelayMs = 0;
    m_loopCount = 0;
    m_loopsDone = 0;
    m_dimWidth = 0;
    m_dimHeight = 0;
    m_valid = false;
    m_animated = false;
}

bool ImageEngine::open(const QString& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    dropImage();

    if (!QFile::exists(path)) {
        qWarning() << "ImageEngine: file does not exist:" << path;
        return false;
    }

    QImageReader reader(path);
    // Only formats our lister routes here should reach the engine; a file that
    // QImageReader cannot decode returns false so the caller falls back (to the
    // MuPDF engine) instead of showing a blank document.
    if (!reader.canRead()) {
        qWarning() << "ImageEngine: cannot decode" << path;
        return false;
    }

    m_path = path;

    // Frame count BEFORE the first read: for an animated format the count scans
    // the whole stream (Qt's GIF handler), which is independent of any read.
    const int frameCount = reader.imageCount();
    if (frameCount < 1) {
        qWarning() << "ImageEngine: no readable frames in" << path;
        dropImage();
        return false;
    }
    m_frameCount = frameCount;

    // Decode the first frame. Its pixels are not kept; the decode proves the
    // file is readable and yields the page/canvas dimensions.
    QImage first = reader.read();
    if (first.isNull()) {
        qWarning() << "ImageEngine: cannot decode" << path;
        dropImage();
        return false;
    }
    m_dimWidth = first.width();
    m_dimHeight = first.height();

    // Animation metadata. Only GIFs animate in place: a multi-frame TIFF stays
    // a single composite of its first frame (see design.md - Goals). Qt's GIF
    // handler reports per-frame delays via nextImageDelay() and a loop count
    // where 0 / -1 mean "loop forever"; a finite positive count is honored.
    const QString format = QFileInfo(path).suffix().toLower();
    if (format == QLatin1String("gif") && m_frameCount > 1) {
        m_animated = true;
        m_frameDelayMs = qMax(0, reader.nextImageDelay());
        const int loops = reader.loopCount();
        m_loopCount = loops > 0 ? loops : -1;
    }

    m_valid = true;
    qDebug() << "ImageEngine:" << m_frameCount << "frame(s), animated:" << m_animated
             << "for" << path;
    return true;
}

void ImageEngine::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    dropImage();
}

bool ImageEngine::isOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_valid;
}

int ImageEngine::pageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_valid ? 1 : 0;
}

QImage ImageEngine::readFrameImage(QImageReader& reader, int frameIndex) const {
    // Qt's GIF handler only advances its internal position via read(); there is
    // no random access (jumpToImage/jumpToNextImage are no-ops for GIF). Decode
    // frames 0..frameIndex sequentially from a fresh reader and keep the last
    // one; the handler composites dependent frames correctly across the run.
    QImage img;
    for (int i = 0; i <= frameIndex; ++i) {
        img = reader.read();
        if (img.isNull())
            break;
    }
    return img;
}

QImage ImageEngine::finishImage(QImage img, float zoom, float dpiScale, int rotation) const {
    if (img.isNull())
        return {};
    img = img.convertToFormat(QImage::Format_RGB888);

    const float scale = zoom * dpiScale;
    if (scale != 1.0f) {
        img = img.scaled(qMax(1, qRound(img.width() * scale)),
                         qMax(1, qRound(img.height() * scale)),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    if (rotation) {
        QTransform t;
        t.translate(img.width() / 2.0, img.height() / 2.0);
        t.rotate(rotation);
        t.translate(-img.width() / 2.0, -img.height() / 2.0);
        img = img.transformed(t, Qt::SmoothTransformation);
    }
    return img;
}

QImage ImageEngine::renderPage(int page, float zoom, float dpiScale, int rotation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_valid || page != 1)
        return {};

    QImageReader reader(m_path);
    QImage frame;
    if (m_animated && m_frameCount > 1)
        frame = readFrameImage(reader, m_currentFrame);
    else
        frame = reader.read();
    return finishImage(frame, zoom, dpiScale, rotation);
}

QString ImageEngine::extractText(int page) {
    Q_UNUSED(page)
    return {};
}

QString ImageEngine::metadata(const QString& key) const {
    Q_UNUSED(key)
    return {};
}

QVector<OutlineItem> ImageEngine::outline() const {
    return {};
}

PageInfo ImageEngine::pageDimensions(int page) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_valid || page != 1 || m_dimWidth <= 0 || m_dimHeight <= 0)
        return {};
    return {m_dimWidth, m_dimHeight};
}

bool ImageEngine::isAnimated() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_animated;
}

int ImageEngine::frameDelayMs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_frameDelayMs;
}

bool ImageEngine::advanceFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_animated || m_frameCount < 2)
        return false;

    const int next = m_currentFrame + 1;
    if (next >= m_frameCount) {
        // Wrapped past the last frame: one full loop done.
        if (m_loopCount > 0) {
            ++m_loopsDone;
            if (m_loopsDone >= m_loopCount)
                return false; // declared loop count exhausted; stop playback
        }
        m_currentFrame = 0;
    } else {
        m_currentFrame = next;
    }

    // The delay of the frame now current comes from a fresh reader positioned
    // at it (GIF handlers expose the just-decoded frame's delay via
    // nextImageDelay() after read()).
    QImageReader reader(m_path);
    readFrameImage(reader, m_currentFrame);
    m_frameDelayMs = qMax(0, reader.nextImageDelay());
    return true;
}