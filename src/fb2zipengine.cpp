#include "fb2zipengine.h"
#include "naturalsort.h"

#include <archive.h>
#include <archive_entry.h>

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QDebug>

#include <algorithm>

namespace {

// Upper bound for a single extracted FB2 entry. Real-world .fb2 files stay in
// the low single-digit MB (base64 images included); anything near this cap is
// either a zip bomb or content we do not want to claim.
constexpr la_int64_t kMaxFb2Bytes = 64ll * 1024 * 1024;

// Window of the candidate entry's head sniffed for the FictionBook root
// element, mirroring MuPDF's own fb2doc_recognize_content (fb2 handler).
constexpr int kSniffBytes = 4096;

bool isFb2Entry(const QString& name) {
    return name.endsWith(QLatin1String(".fb2"), Qt::CaseInsensitive);
}

bool looksLikeFb2(const QByteArray& data) {
    return data.left(kSniffBytes).contains("<FictionBook");
}

// Virtual filename handed to MuPDF as the format magic: the entry name with
// any directory prefix stripped ("books/sample.fb2" -> "sample.fb2") so the
// trailing ".fb2" extension selects the FictionBook handler regardless of how
// the FB2 is packed inside the zip.
QString magicNameOf(const QString& entryName) {
    const int slash = qMax(entryName.lastIndexOf('/'), entryName.lastIndexOf('\\'));
    return slash < 0 ? entryName : entryName.mid(slash + 1);
}

// Opens the archive and returns every *.fb2 entry in natural order (payload
// data skipped; directories, symlinks and decryption-failed headers skipped).
// Empty when the file is not an archive or holds no FB2 entry.
QStringList listFb2Entries(const QString& path) {
    struct archive* ar = ::archive_read_new();
    if (!ar)
        return {};
    ::archive_read_support_filter_all(ar);
    ::archive_read_support_format_all(ar);

    int r =
#ifdef _WIN32
        ::archive_read_open_filename_w(ar, reinterpret_cast<const wchar_t*>(path.utf16()), 0);
#else
        ::archive_read_open_filename(ar, QFile::encodeName(path).constData(), 0);
#endif
    if (r != ARCHIVE_OK) {
        ::archive_read_free(ar);
        return {};
    }

    QStringList names;
    struct archive_entry* entry = nullptr;
    while (::archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
        if (::archive_entry_filetype(entry) == AE_IFDIR) {
            ::archive_read_data_skip(ar);
            continue;
        }
        const char* pn = ::archive_entry_pathname_utf8(entry);
        if (!pn)
            pn = ::archive_entry_pathname(entry);
        if (pn && isFb2Entry(QString::fromUtf8(pn)))
            names.append(QString::fromUtf8(pn));
        ::archive_read_data_skip(ar);
    }
    ::archive_read_free(ar);

    if (names.size() > 1) {
        std::sort(names.begin(), names.end(),
                  [](const QString& a, const QString& b) {
                      return naturalCompare(a, b) < 0;
                  });
    }
    return names;
}

// Extracts one entry's payload with the size cap applied; empty on any error
// (missing entry, decryption failure, exceeded cap).
QByteArray extractEntry(const QString& path, const QString& name) {
    struct archive* ar = ::archive_read_new();
    if (!ar)
        return {};
    ::archive_read_support_filter_all(ar);
    ::archive_read_support_format_all(ar);

    QByteArray data;
#ifdef _WIN32
    int r = ::archive_read_open_filename_w(
        ar, reinterpret_cast<const wchar_t*>(path.utf16()), 0);
#else
    int r = ::archive_read_open_filename(ar, QFile::encodeName(path).constData(), 0);
#endif
    if (r != ARCHIVE_OK) {
        ::archive_read_free(ar);
        return {};
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
        if (pn && name == QString::fromUtf8(pn)) {
            const la_int64_t declared =
                ::archive_entry_size_is_set(entry) ? ::archive_entry_size(entry) : 0;
            if (declared > kMaxFb2Bytes)
                break;
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
                if (data.size() > kMaxFb2Bytes) {
                    data.clear();
                    break;
                }
            }
            break;
        }
        ::archive_read_data_skip(ar);
    }
    ::archive_read_free(ar);
    return data;
}

} // namespace

Fb2ZipEngine::~Fb2ZipEngine() {
    close();
}

bool Fb2ZipEngine::open(const QString& path) {
    close();

    if (!QFile::exists(path)) {
        qWarning() << "Fb2ZipEngine: file does not exist:" << path;
        return false;
    }

    // Skip contract: only claim zips that actually contain a FictionBook
    // document. Every other zip (no .fb2 entry, undecodable archive, encrypted
    // entries, oversized payloads, .fb2-named non-FB2 content) returns false
    // here so ListLoad returns 0 and the host moves on to the next plugin.
    const QStringList candidates = listFb2Entries(path);
    if (candidates.isEmpty()) {
        qDebug() << "Fb2ZipEngine: no FictionBook entry in" << path
                 << "- declining (next plugin)";
        return false;
    }

    for (const QString& candidate : candidates) {
        const QByteArray data = extractEntry(path, candidate);
        if (data.isEmpty() || !looksLikeFb2(data))
            continue;
        // magicNameOf ends in ".fb2" (candidates are filtered on that suffix),
        // so MuPDF's FB2 handler wins deterministically over its zip/CBZ one.
        if (!m_fb2.openBuffer(data, magicNameOf(candidate))) {
            qWarning() << "Fb2ZipEngine: MuPDF failed to open" << candidate
                       << "from" << path;
            continue;
        }
        qDebug() << "Fb2ZipEngine:" << m_fb2.pageCount() << "FB2 pages from"
                 << candidate << "in" << path;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_valid = true;
        return true;
    }

    qWarning() << "Fb2ZipEngine: no usable FictionBook payload in" << path;
    return false;
}

void Fb2ZipEngine::close() {
    m_fb2.close();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_valid = false;
}

bool Fb2ZipEngine::isOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_valid && m_fb2.isOpen();
}

int Fb2ZipEngine::pageCount() const {
    return m_fb2.pageCount();
}

QImage Fb2ZipEngine::renderPage(int page, float zoom, float dpiScale, int rotation) {
    return m_fb2.renderPage(page, zoom, dpiScale, rotation);
}

QString Fb2ZipEngine::extractText(int page) {
    return m_fb2.extractText(page);
}

QString Fb2ZipEngine::metadata(const QString& key) const {
    return m_fb2.metadata(key);
}

QVector<OutlineItem> Fb2ZipEngine::outline() const {
    return m_fb2.outline();
}

PageInfo Fb2ZipEngine::pageDimensions(int page) const {
    return m_fb2.pageDimensions(page);
}

PageText Fb2ZipEngine::pageText(int page) {
    return m_fb2.pageText(page);
}

QVector<LinkItem> Fb2ZipEngine::pageLinks(int page) {
    return m_fb2.pageLinks(page);
}

bool Fb2ZipEngine::supportsSearch() const {
    return m_fb2.supportsSearch();
}

QVector<TextMatch> Fb2ZipEngine::searchText(int page, const QString& needle, bool matchCase) {
    return m_fb2.searchText(page, needle, matchCase);
}