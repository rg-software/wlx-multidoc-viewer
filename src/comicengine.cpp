#include "comicengine.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTransform>
#include <QDebug>

#include <algorithm>

namespace {

bool isImageEntry(const QString& name) {
    return name.endsWith(QLatin1String(".png"), Qt::CaseInsensitive) ||
           name.endsWith(QLatin1String(".jpg"), Qt::CaseInsensitive) ||
           name.endsWith(QLatin1String(".jpeg"), Qt::CaseInsensitive) ||
           name.endsWith(QLatin1String(".gif"), Qt::CaseInsensitive) ||
           name.endsWith(QLatin1String(".bmp"), Qt::CaseInsensitive);
}

// Natural comparison: digit runs compare numerically (case-insensitive), so
// page2 sorts before page10. Everything else compares by lower-cased code
// point. Returns -1/0/1.
int naturalCompare(const QString& a, const QString& b) {
    int i = 0, j = 0;
    const int na = a.size(), nb = b.size();
    while (i < na && j < nb) {
        const QChar ca = a[i], cb = b[j];
        const bool da = ca.isDigit(), db = cb.isDigit();
        if (da && db) {
            int ia = i, ib = j;
            while (ia < na && a[ia].isDigit()) ++ia;
            while (ib < nb && b[ib].isDigit()) ++ib;
            // strip leading zeros
            while (i < ia - 1 && a[i] == QLatin1Char('0')) ++i;
            while (j < ib - 1 && b[j] == QLatin1Char('0')) ++j;
            const int la = ia - i, lb = ib - j;
            if (la != lb)
                return la < lb ? -1 : 1;
            for (int k = 0; k < la; ++k) {
                if (a[i + k] != b[j + k])
                    return a[i + k] < b[j + k] ? -1 : 1;
            }
            i = ia;
            j = ib;
        } else {
            const QChar la = ca.toLower(), lb = cb.toLower();
            if (la != lb)
                return la < lb ? -1 : 1;
            ++i;
            ++j;
        }
    }
    if (i < na) return 1;
    if (j < nb) return -1;
    return 0;
}

} // namespace

ComicEngine::~ComicEngine() {
    close();
}

bool ComicEngine::open(const QString& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    dropArchive();

    if (!QFile::exists(path)) {
        qWarning() << "ComicEngine: file does not exist:" << path;
        return false;
    }
    m_path = path;

    // Probe pass: enumerate image entries, skipping payload data. The archive
    // is forward-only, so renders reopen it per entry (same trade-off as the
    // CHM engine's per-page document).
    struct archive* ar = ::archive_read_new();
    if (!ar) {
        qWarning() << "ComicEngine: failed to create libarchive reader";
        return false;
    }
    ::archive_read_support_filter_all(ar);
    ::archive_read_support_format_all(ar);

    int r =
#ifdef _WIN32
        ::archive_read_open_filename_w(ar, reinterpret_cast<const wchar_t*>(path.utf16()), 0);
#else
        ::archive_read_open_filename(ar, QFile::encodeName(path).constData(), 0);
#endif
    if (r != ARCHIVE_OK) {
        qWarning() << "ComicEngine: cannot open" << path << "-"
                   << ::archive_error_string(ar);
        ::archive_read_free(ar);
        return false;
    }

    struct archive_entry* entry = nullptr;
    while (::archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
        if (::archive_entry_filetype(entry) == AE_IFDIR) {
            ::archive_read_data_skip(ar);
            continue;
        }
        const char* pn = ::archive_entry_pathname_utf8(entry);
        if (!pn)
            pn = ::archive_entry_pathname(entry);
        if (pn && isImageEntry(QString::fromUtf8(pn)))
            m_pages.append(QString::fromUtf8(pn));
        ::archive_read_data_skip(ar);
    }
    const bool readable = true; // header walk completed without fatal error
    ::archive_read_free(ar);

    if (m_pages.size() > 1) {
        std::sort(m_pages.begin(), m_pages.end(),
                  [](const QString& a, const QString& b) {
                      return naturalCompare(a, b) < 0;
                  });
    }

    m_valid = readable;
    qDebug() << "ComicEngine:" << m_pages.size() << "image pages for" << path;
    return true;
}

void ComicEngine::dropArchive() {
    m_valid = false;
    m_pages.clear();
    m_dimCache.clear();
    m_path.clear();
}

void ComicEngine::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    dropArchive();
}

bool ComicEngine::isOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_valid;
}

int ComicEngine::pageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pages.size();
}

QByteArray ComicEngine::extractEntry(const QString& name) const {
    struct archive* ar = ::archive_read_new();
    if (!ar)
        return {};
    ::archive_read_support_filter_all(ar);
    ::archive_read_support_format_all(ar);

    QByteArray data;
#ifdef _WIN32
    int r = ::archive_read_open_filename_w(
        ar, reinterpret_cast<const wchar_t*>(m_path.utf16()), 0);
#else
    int r = ::archive_read_open_filename(ar, QFile::encodeName(m_path).constData(), 0);
#endif
    if (r != ARCHIVE_OK) {
        ::archive_read_free(ar);
        return {};
    }

    struct archive_entry* entry = nullptr;
    while (::archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
        const char* pn = ::archive_entry_pathname_utf8(entry);
        if (!pn)
            pn = ::archive_entry_pathname(entry);
        if (pn && name == QString::fromUtf8(pn)) {
            const void* block = nullptr;
            size_t size = 0;
            la_int64_t offset = 0;
            while (true) {
                r = ::archive_read_data_block(ar, &block, &size, &offset);
                if (r == ARCHIVE_EOF)
                    break;
                if (r < ARCHIVE_OK) {
                    data.clear();
                    break;
                }
                data.append(static_cast<const char*>(block), static_cast<int>(size));
            }
            break;
        }
        ::archive_read_data_skip(ar);
    }
    ::archive_read_free(ar);
    return data;
}

QImage ComicEngine::renderPage(int page, float zoom, float dpiScale, int rotation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return renderPageLocked(page, zoom, dpiScale, rotation);
}

QImage ComicEngine::renderPageLocked(int page, float zoom, float dpiScale, int rotation) const {
    if (!m_valid || page < 1 || page > m_pages.size())
        return {};

    const QByteArray bytes = extractEntry(m_pages.at(page - 1));
    QImage img = QImage::fromData(bytes);
    if (img.isNull()) {
        qWarning() << "ComicEngine: cannot decode" << m_pages.at(page - 1);
        return {};
    }
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

QString ComicEngine::extractText(int page) {
    Q_UNUSED(page)
    return {};
}

QString ComicEngine::metadata(const QString& key) const {
    Q_UNUSED(key)
    return {};
}

QVector<OutlineItem> ComicEngine::outline() const {
    return {};
}

PageInfo ComicEngine::pageDimensions(int page) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_valid || page < 1 || page > m_pages.size())
        return {};

    auto it = m_dimCache.constFind(page);
    if (it != m_dimCache.cend())
        return it.value();

    PageInfo info;
    const QImage img = renderPageLocked(page, 1.0f, 1.0f, 0);
    if (!img.isNull()) {
        info.width = img.width();
        info.height = img.height();
        m_dimCache.insert(page, info);
    }
    return info;
}
