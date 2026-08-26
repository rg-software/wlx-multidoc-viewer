#include "chmengine.h"

#include <mupdf/fitz.h>

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryFile>
#include <QDebug>
#include <QtEndian>
#include <functional>

#include <chm_lib.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// CHM entry paths are ASCII per the ITSFS spec.
QString normalizePath(const QString& path) {
    QString p = path;
    p.replace(QLatin1Char('\\'), QLatin1Char('/'));
    // TOC links may carry #fragment anchors; the archive path excludes them.
    const int hash = p.indexOf(QLatin1Char('#'));
    if (hash >= 0)
        p.truncate(hash);
    p = p.toLower();
    while (p.startsWith(QLatin1Char('/')))
        p.remove(0, 1);
    return p;
}

bool isHtmlPath(const QString& path) {
    return path.endsWith(QLatin1String(".htm"), Qt::CaseInsensitive) ||
           path.endsWith(QLatin1String(".html"), Qt::CaseInsensitive);
}

int enumCallback(struct chmFile*, struct chmUnitInfo* ui, void* context) {
    auto* pages = static_cast<QVector<QString>*>(context);
    if (!(ui->flags & CHM_ENUMERATE_NORMAL) ||
        (ui->flags & (CHM_ENUMERATE_SPECIAL | CHM_ENUMERATE_META)))
        return CHM_ENUMERATOR_CONTINUE;
    const QString path = QString::fromLatin1(ui->path);
    if (isHtmlPath(path))
        pages->append(path);
    return CHM_ENUMERATOR_CONTINUE;
}

quint32 le32(const unsigned char* p) { return qFromLittleEndian<quint32>(p); }
quint16 le16(const unsigned char* p) { return qFromLittleEndian<quint16>(p); }

// Full LCID -> Windows ANSI codepage mapping (SumatraPDF-style). Ambiguous
// languages are resolved by full LCID first; the rest fall through to the
// primary language id. Unknown LCIDs default to 1252.
int lcidToCodepage(quint32 lcid) {
    switch (static_cast<quint16>(lcid)) {
        case 0x0404:
        case 0x0C04:
        case 0x1404: return 950;   // Chinese Traditional
        case 0x0804:
        case 0x1004: return 936;   // Chinese Simplified
        case 0x081A:
        case 0x181A: return 1250;  // Serbian Latin
        case 0x0C1A:
        case 0x1C1A: return 1251;  // Serbian Cyrillic
        case 0x042C: return 1254;  // Azerbaijani Latin
        case 0x082C: return 1251;  // Azerbaijani Cyrillic
        case 0x0428: return 1251;  // Tajik
        case 0x0443: return 1251;  // Uzbek Cyrillic
        case 0x0843: return 1254;  // Uzbek Latin
        default: break;
    }
    switch (static_cast<quint16>(lcid) & 0x03FF) {
        case 0x01: return 1256;    // Arabic
        case 0x02: return 1251;    // Bulgarian
        case 0x03: return 1252;    // Catalan
        case 0x04: return 936;     // Chinese (default)
        case 0x05: return 1250;    // Czech
        case 0x06: return 1252;    // Danish
        case 0x07: return 1252;    // German
        case 0x08: return 1253;    // Greek
        case 0x09: return 1252;    // English
        case 0x0A: return 1252;    // Spanish
        case 0x0B: return 1252;    // Finnish
        case 0x0C: return 1252;    // French
        case 0x0D: return 1255;    // Hebrew
        case 0x0E: return 1250;    // Hungarian
        case 0x0F: return 1252;    // Icelandic
        case 0x10: return 1252;    // Italian
        case 0x11: return 932;     // Japanese
        case 0x12: return 949;     // Korean
        case 0x13: return 1252;    // Dutch
        case 0x14: return 1252;    // Norwegian
        case 0x15: return 1250;    // Polish
        case 0x16: return 1252;    // Portuguese
        case 0x17: return 1252;    // Romansh
        case 0x18: return 1250;    // Romanian
        case 0x19: return 1251;    // Russian
        case 0x1A: return 1250;    // Croatian / Serbian
        case 0x1B: return 1250;    // Slovak
        case 0x1C: return 1250;    // Albanian
        case 0x1D: return 1252;    // Swedish
        case 0x1E: return 874;     // Thai
        case 0x1F: return 1254;    // Turkish
        case 0x20: return 1256;    // Urdu
        case 0x21: return 1252;    // Indonesian
        case 0x22: return 1251;    // Ukrainian
        case 0x23: return 1251;    // Belarusian
        case 0x24: return 1250;    // Slovenian
        case 0x25: return 1257;    // Estonian
        case 0x26: return 1257;    // Latvian
        case 0x27: return 1257;    // Lithuanian
        case 0x28: return 1251;    // Tajik fallback
        case 0x29: return 1256;    // Farsi
        case 0x2A: return 1258;    // Vietnamese
        case 0x2B: return 1252;    // Armenian
        case 0x2C: return 1254;    // Azerbaijani
        case 0x2F: return 1251;    // Macedonian
        case 0x43: return 1251;    // Uzbek
        default:   return 1252;
    }
}

constexpr quint64 kMaxEntryBytes = 64ull * 1024 * 1024;

// Bounding rect of one fz_quad in page space (y-down), matching the
// convention used for word bboxes elsewhere.
QRectF quadRect(const fz_quad& q) {
    return QRectF(QPointF(q.ul.x, q.ul.y), QPointF(q.lr.x, q.lr.y)).normalized();
}

// One page-space rect carrying its reading-order line index.
struct SearchGlyph {
    QChar c;
    QRectF box;
    int line = 0;
};

// --- .hhc table-of-contents parsing ---------------------------------------

struct HhcNode {
    QString name;
    QString local;
    QVector<HhcNode> children;
};

struct HhcRecord {
    int depth = 0;
    QString name;
    QString local;
};

QString decodeEntities(QString s) {
    if (!s.contains(QLatin1Char('&')))
        return s;
    int i = 0;
    while ((i = s.indexOf(QLatin1Char('&'), i)) >= 0) {
        const int sc = s.indexOf(QLatin1Char(';'), i);
        if (sc < 0 || sc - i > 12) {
            ++i;
            continue;
        }
        const QString ent = s.mid(i + 1, sc - i - 1);
        QChar replacement;
        bool ok = false;
        if (ent.startsWith(QLatin1Char('#'))) {
            int cp = 0;
            if (ent.size() > 2 && (ent[1] == QLatin1Char('x') || ent[1] == QLatin1Char('X')))
                cp = ent.mid(2).toInt(&ok, 16);
            else
                cp = ent.mid(1).toInt(&ok, 10);
            if (ok && cp > 0 && cp <= 0xFFFF)
                replacement = QChar(static_cast<char16_t>(cp));
            else
                ok = false;
        } else if (ent.compare(QLatin1String("amp"), Qt::CaseInsensitive) == 0) {
            replacement = QLatin1Char('&'); ok = true;
        } else if (ent.compare(QLatin1String("lt"), Qt::CaseInsensitive) == 0) {
            replacement = QLatin1Char('<'); ok = true;
        } else if (ent.compare(QLatin1String("gt"), Qt::CaseInsensitive) == 0) {
            replacement = QLatin1Char('>'); ok = true;
        } else if (ent.compare(QLatin1String("quot"), Qt::CaseInsensitive) == 0) {
            replacement = QLatin1Char('"'); ok = true;
        } else if (ent.compare(QLatin1String("apos"), Qt::CaseInsensitive) == 0) {
            replacement = QLatin1Char('\''); ok = true;
        } else if (ent.compare(QLatin1String("nbsp"), Qt::CaseInsensitive) == 0) {
            replacement = QLatin1Char(' '); ok = true;
        }
        if (!ok) {
            ++i;
            continue;
        }
        s.replace(i, sc - i + 1, replacement);
        ++i;
    }
    return s;
}

// Case-insensitive attribute lookup inside one tag's interior text; returns
// the raw-cased value (quoted or unquoted).
QString attrValue(const QString& tagInterior, const char* attr) {
    const QString needle = QString::fromLatin1(attr);
    int i = 0;
    while ((i = tagInterior.indexOf(needle, i, Qt::CaseInsensitive)) >= 0) {
        const bool boundaryLeft =
            i == 0 || !(tagInterior[i - 1].isLetterOrNumber() || tagInterior[i - 1] == QLatin1Char('-'));
        int j = i + needle.size();
        if (!boundaryLeft)
            continue;
        while (j < tagInterior.size() && tagInterior[j].isSpace())
            ++j;
        if (j >= tagInterior.size() || tagInterior[j] != QLatin1Char('=')) {
            continue;
        }
        ++j;
        while (j < tagInterior.size() && tagInterior[j].isSpace())
            ++j;
        if (j >= tagInterior.size())
            return {};
        const QChar quote = tagInterior[j];
        if (quote == QLatin1Char('"') || quote == QLatin1Char('\'')) {
            const int close = tagInterior.indexOf(quote, j + 1);
            if (close < 0)
                return {};
            return tagInterior.mid(j + 1, close - j - 1);
        }
        int end = j;
        while (end < tagInterior.size() && !tagInterior[end].isSpace() &&
               tagInterior[end] != QLatin1Char('>'))
            ++end;
        return tagInterior.mid(j, end - j);
    }
    return {};
}

QVector<HhcRecord> collectHhcRecords(const QString& html) {
    QVector<HhcRecord> recs;
    const QString lower = html.toLower();
    int depth = 0;
    int pos = 0;
    bool inObject = false;
    QString objName;
    QString objLocal;

    auto tagNameOf = [](const QString& interior) {
        QString n;
        for (QChar c : interior) {
            if (!(c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('/')))
                break;
            n.append(c);
        }
        return n;
    };

    while (true) {
        const int lt = lower.indexOf(QLatin1Char('<'), pos);
        if (lt < 0)
            break;
        if (lower.indexOf(QLatin1String("<!--"), lt) == lt) {
            const int ce = lower.indexOf(QLatin1String("-->"), lt + 4);
            pos = ce < 0 ? lower.size() : ce + 3;
            continue;
        }
        const int gt = lower.indexOf(QLatin1Char('>'), lt);
        if (gt < 0)
            break;
        const QString interior = lower.mid(lt + 1, gt - lt - 1);
        const QString raw = html.mid(lt + 1, gt - lt - 1);
        pos = gt + 1;

        const QString name = tagNameOf(interior);
        const bool closing = name.startsWith(QLatin1Char('/'));
        const QString base = closing ? name.mid(1) : name;

        if (base == QLatin1String("ul")) {
            depth += closing ? -1 : 1;
            if (depth < 0)
                depth = 0;
        } else if (base == QLatin1String("object")) {
            if (closing) {
                if (inObject) {
                    HhcRecord r;
                    r.depth = depth;
                    r.name = objName.trimmed();
                    r.local = objLocal.trimmed();
                    if (!r.name.isEmpty() || !r.local.isEmpty())
                        recs.append(r);
                    inObject = false;
                }
            } else if (attrValue(interior, "type")
                           .compare(QLatin1String("text/sitemap"), Qt::CaseInsensitive) == 0) {
                inObject = true;
                objName.clear();
                objLocal.clear();
            }
        } else if (base == QLatin1String("param") && inObject && !closing) {
            const QString pn = attrValue(interior, "name").toLower().trimmed();
            const QString pv = decodeEntities(attrValue(raw, "value"));
            if (pn == QLatin1String("name"))
                objName = pv;
            else if (pn == QLatin1String("local"))
                objLocal = pv;
        }
    }
    return recs;
}

// Records arrive in pre-order; build the nested tree by consuming each depth
// level recursively so QVector growth never invalidates parent references.
void buildHhcLevel(const QVector<HhcRecord>& recs, int& i, int depth, QVector<HhcNode>& out) {
    while (i < recs.size()) {
        if (recs[i].depth < depth)
            return;
        HhcNode node;
        node.name = recs[i].name;
        node.local = recs[i].local;
        ++i;
        if (i < recs.size() && recs[i].depth > depth)
            buildHhcLevel(recs, i, depth + 1, node.children);
        out.append(std::move(node));
    }
}

QVector<HhcNode> parseHhc(const QString& html) {
    if (html.isEmpty())
        return {};
    const QVector<HhcRecord> recs = collectHhcRecords(html);
    if (recs.isEmpty())
        return {};
    QVector<HhcNode> roots;
    int i = 0;
    buildHhcLevel(recs, i, recs.first().depth, roots);
    return roots;
}

} // namespace

void ChmEngine::OpenedHtmlPage::drop(fz_context* ctx) {
    if (page) {
        fz_drop_page(ctx, page);
        page = nullptr;
    }
    if (doc) {
        fz_drop_document(ctx, doc);
        doc = nullptr;
    }
}

ChmEngine::~ChmEngine() {
    close();
}

bool ChmEngine::open(const QString& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    dropArchive();

    if (!QFile::exists(path)) {
        qWarning() << "ChmEngine: file does not exist:" << path;
        return false;
    }

#ifdef _WIN32
    // chm_open resolves the file through the narrow ANSI API (CreateFileA),
    // so stage a temp copy when the path is not representable in the system
    // codepage; otherwise open in place.
    QByteArray nameBytes = path.toLocal8Bit();
    if (QString::fromLocal8Bit(nameBytes) != path) {
        auto staged = std::make_unique<QTemporaryFile>(
            QDir::temp().filePath("wlx-chm-XXXXXX.chm"));
        QFile src(path);
        if (!staged->open() || !src.open(QIODevice::ReadOnly)) {
            qWarning() << "ChmEngine: failed to stage Unicode-named CHM:" << path;
            return false;
        }
        staged->write(src.readAll());
        staged->flush();
        nameBytes = staged->fileName().toLocal8Bit();
        m_stagedFile = std::move(staged);
        qDebug() << "ChmEngine: staged" << path << "->" << m_stagedFile->fileName();
    }
#else
    const QByteArray nameBytes = QFile::encodeName(path);
#endif
    m_chm = ::chm_open(nameBytes.constData());
    if (!m_chm) {
        qWarning() << "ChmEngine: chm_open failed for" << path;
        return false;
    }

    ::chm_enumerate(m_chm, CHM_ENUMERATE_ALL, enumCallback, &m_htmlPages);

    m_fzCtx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (m_fzCtx)
        fz_register_document_handlers(m_fzCtx);
    else
        qWarning() << "ChmEngine: failed to create MuPDF context for" << path;

    parseSystemData();
    composeDocument();

    qDebug() << "ChmEngine:" << m_htmlPages.size() << "HTML pages," << m_outline.size()
             << "outline items, codepage" << m_codepage << "for" << path;
    return true;
}

void ChmEngine::dropArchive() {
    if (m_fzCtx) {
        fz_drop_context(m_fzCtx);
        m_fzCtx = nullptr;
    }
    if (m_chm) {
        ::chm_close(m_chm);
        m_chm = nullptr;
    }
    m_stagedFile.reset();
    m_htmlPages.clear();
    m_outline.clear();
    m_dimCache.clear();
    m_title.clear();
    m_creator.clear();
    m_systemHome.clear();
    m_systemToc.clear();
    m_codepage = 1252;
}

void ChmEngine::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    dropArchive();
}

bool ChmEngine::isOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_chm != nullptr;
}

int ChmEngine::pageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_htmlPages.size();
}

QByteArray ChmEngine::readEntry(const QString& path) const {
    if (!m_chm)
        return {};
    // Control-file strings (/#STRINGS, /#SYSTEM) store archive paths without
    // the leading slash; chm_resolve_object requires it.
    QString fixed = path;
    fixed.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (!fixed.isEmpty() && fixed.startsWith(QLatin1Char('/')))
        fixed.remove(0, 1);
    const QByteArray pathBytes = ('/' + fixed).toUtf8();
    struct chmUnitInfo ui {};
    if (::chm_resolve_object(m_chm, pathBytes.constData(), &ui) != CHM_RESOLVE_SUCCESS)
        return {};
    if (ui.length == 0 || ui.length > kMaxEntryBytes)
        return {};
    QByteArray data;
    data.resize(static_cast<int>(ui.length));
    const long long got = ::chm_retrieve_object(
        m_chm, &ui, reinterpret_cast<unsigned char*>(data.data()), 0,
        static_cast<unsigned long long>(ui.length));
    if (got <= 0)
        return {};
    data.resize(static_cast<int>(got));
    return data;
}

ChmEngine::OpenedHtmlPage ChmEngine::openHtmlPage(int page) const {
    OpenedHtmlPage opened;
    if (!m_chm || !m_fzCtx || page < 1 || page > m_htmlPages.size())
        return opened;

    QByteArray html = readEntry(m_htmlPages.at(page - 1));
    if (html.isEmpty())
        return opened;

    // MuPDF's HTML pipeline assumes UTF-8 input; transcode non-UTF-8 pages on
    // Windows where the codepage is known. Elsewhere prepend a charset hint as
    // a best-effort fallback (spec acknowledges mojibake outside 1252/ACP).
    if (m_codepage != 65001) {
#ifdef _WIN32
        html = decodeText(html).toUtf8();
#else
        const QByteArray meta =
            "<meta charset=\"windows-" + QByteArray::number(m_codepage) + "\">";
        html.prepend(meta);
#endif
    }

    fz_context* ctx = m_fzCtx;
    fz_buffer* buf = nullptr;
    fz_try(ctx) {
        buf = fz_new_buffer_from_copied_data(
            ctx, reinterpret_cast<const unsigned char*>(html.constData()),
            static_cast<size_t>(html.size()));
        opened.doc = fz_open_document_with_buffer(ctx, "html", buf);
        if (opened.doc)
            opened.page = fz_load_page(ctx, opened.doc, 0);
    }
    fz_always(ctx) {
        if (buf)
            fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        qWarning() << "ChmEngine: failed to open HTML page" << page;
        opened.doc = nullptr;
        opened.page = nullptr;
    }
    return opened;
}

QImage ChmEngine::renderPage(int page, float zoom, float dpiScale, int rotation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return renderPageLocked(page, zoom, dpiScale, rotation);
}

QImage ChmEngine::renderPageLocked(int page, float zoom, float dpiScale, int rotation) const {
    OpenedHtmlPage h = openHtmlPage(page);
    if (!h.doc || !h.page)
        return {};

    fz_context* ctx = m_fzCtx;
    fz_pixmap* pixmap = nullptr;
    QImage result;
    const float effectiveZoom = zoom * dpiScale;

    fz_try(ctx) {
        // Same transform pipeline as MuPdfEngine::renderPage.
        fz_rect bounds = fz_bound_page(ctx, h.page);
        fz_matrix ctm = fz_scale(effectiveZoom, effectiveZoom);
        if (rotation) {
            float cx = (bounds.x0 + bounds.x1) * 0.5f;
            float cy = (bounds.y0 + bounds.y1) * 0.5f;
            ctm = fz_concat(ctm, fz_translate(cx, cy));
            ctm = fz_concat(ctm, fz_rotate(static_cast<float>(rotation)));
            ctm = fz_concat(ctm, fz_translate(-cx, -cy));
        }

        pixmap = fz_new_pixmap_from_page(ctx, h.page, ctm, fz_device_rgb(ctx), 0);
        if (pixmap && pixmap->w > 0 && pixmap->h > 0) {
            QImage img(pixmap->samples, pixmap->w, pixmap->h, pixmap->w * 3,
                       QImage::Format_RGB888);
            result = img.copy();
        }
    }
    fz_always(ctx) {
        if (pixmap)
            fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx) {
        qWarning() << "ChmEngine: HTML render failed for page" << page;
        result = QImage();
    }

    h.drop(ctx);
    return result;
}

PageText ChmEngine::pageText(int page) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PageText result;
    OpenedHtmlPage h = openHtmlPage(page);
    if (!h.doc || !h.page)
        return result;

    fz_context* ctx = m_fzCtx;
    fz_stext_page* stext = nullptr;

    fz_try(ctx) {
        fz_stext_options opts;
        opts.flags = FZ_STEXT_ACCURATE_BBOXES;
        stext = fz_new_stext_page_from_page(ctx, h.page, &opts);

        int lineIndex = 0;
        for (fz_stext_block* block = stext->first_block; block; block = block->next) {
            if (block->type != FZ_STEXT_BLOCK_TEXT)
                continue;
            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                QString wordText;
                QRectF wordBox;
                bool building = false;
                bool lineHasChars = false;
                auto flushWord = [&]() {
                    if (!building)
                        return;
                    TextWord w;
                    w.text = wordText;
                    w.bbox = wordBox;
                    w.lineIndex = lineIndex;
                    result.words.append(w);
                    wordText.clear();
                    wordBox = QRectF();
                    building = false;
                };
                for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                    lineHasChars = true;
                    const QChar qc(ch->c);
                    if (qc.isSpace()) {
                        flushWord();
                        continue;
                    }
                    const QRectF r = quadRect(ch->quad);
                    if (!building) {
                        wordBox = r;
                        building = true;
                    } else {
                        wordBox = wordBox.united(r);
                    }
                    wordText.append(qc);
                }
                flushWord();
                if (lineHasChars)
                    ++lineIndex;
            }
        }
        result.hasText = !result.words.isEmpty();
    }
    fz_always(ctx) {
        if (stext)
            fz_drop_stext_page(ctx, stext);
    }
    fz_catch(ctx) {
        result = PageText{};
    }

    h.drop(ctx);
    return result;
}

QString ChmEngine::extractText(int page) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QString result;
    OpenedHtmlPage h = openHtmlPage(page);
    if (!h.doc || !h.page)
        return result;

    fz_context* ctx = m_fzCtx;
    fz_stext_page* stext = nullptr;

    fz_try(ctx) {
        fz_stext_options opts;
        opts.flags = FZ_STEXT_ACCURATE_BBOXES;
        stext = fz_new_stext_page_from_page(ctx, h.page, &opts);

        QByteArray textBuf;
        for (fz_stext_block* block = stext->first_block; block; block = block->next) {
            if (block->type != FZ_STEXT_BLOCK_TEXT)
                continue;
            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                    char buf8[8];
                    const int n = fz_runetochar(buf8, ch->c);
                    textBuf.append(buf8, n);
                }
                textBuf.append('\n');
            }
        }
        result = QString::fromUtf8(textBuf);
    }
    fz_always(ctx) {
        if (stext)
            fz_drop_stext_page(ctx, stext);
    }
    fz_catch(ctx) {
        result.clear();
    }

    h.drop(ctx);
    return result;
}

QVector<TextMatch> ChmEngine::searchText(int page, const QString& needle, bool matchCase) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QVector<TextMatch> results;
    if (needle.isEmpty())
        return results;

    OpenedHtmlPage h = openHtmlPage(page);
    if (!h.doc || !h.page)
        return results;

    fz_context* ctx = m_fzCtx;
    fz_stext_page* stext = nullptr;
    float pageWidth = 0.0f;
    float pageHeight = 0.0f;
    QVector<QRectF> rects;

    const Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const QString needleNorm = matchCase ? needle : needle.toLower();

    fz_try(ctx) {
        const fz_rect bounds = fz_bound_page(ctx, h.page);
        pageWidth = bounds.x1 - bounds.x0;
        pageHeight = bounds.y1 - bounds.y0;
        if (pageWidth >= 1.0f && pageHeight >= 1.0f) {
            fz_stext_options opts;
            opts.flags = FZ_STEXT_ACCURATE_BBOXES;
            stext = fz_new_stext_page_from_page(ctx, h.page, &opts);

            QVector<SearchGlyph> glyphs;
            int lineNo = 0;
            for (fz_stext_block* block = stext->first_block; block; block = block->next) {
                if (block->type != FZ_STEXT_BLOCK_TEXT)
                    continue;
                for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                    for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                        const QChar c(ch->c);
                        if (c.isSpace())
                            continue;
                        glyphs.append(SearchGlyph{c, quadRect(ch->quad), lineNo});
                    }
                    ++lineNo;
                }
            }

            if (!glyphs.isEmpty()) {
                QString run;
                QVector<int> runGlyph;
                run.reserve(glyphs.size());
                runGlyph.reserve(glyphs.size());
                int lastLine = glyphs.first().line;
                for (int i = 0; i < glyphs.size(); ++i) {
                    const SearchGlyph& g = glyphs[i];
                    if (i > 0 && g.line != lastLine) {
                        run.append(QLatin1Char(' '));
                        runGlyph.append(-1);
                    }
                    run.append(g.c);
                    runGlyph.append(i);
                    lastLine = g.line;
                }
                const QString runNorm = matchCase ? run : run.toLower();

                int from = 0;
                while (from <= run.size()) {
                    const int hit = runNorm.indexOf(needleNorm, from, cs);
                    if (hit < 0)
                        break;
                    QRectF box;
                    const int last = hit + needle.size();
                    for (int i = hit; i < last; ++i) {
                        const int gi = runGlyph[i];
                        if (gi >= 0 && gi < glyphs.size()) {
                            const QRectF r = glyphs[gi].box;
                            box = box.isNull() ? r : box.united(r);
                        }
                    }
                    if (!box.isNull()) {
                        rects.append(QRectF(box.x() / pageWidth,
                                            box.y() / pageHeight,
                                            box.width() / pageWidth,
                                            box.height() / pageHeight));
                    }
                    from = hit + 1;
                }
            }
        }
    }
    fz_always(ctx) {
        if (stext)
            fz_drop_stext_page(ctx, stext);
    }
    fz_catch(ctx) {
        rects.clear();
    }

    h.drop(ctx);

    if (!rects.isEmpty()) {
        TextMatch match;
        match.page = page;
        match.rects = std::move(rects);
        results.append(match);
    }
    return results;
}

QString ChmEngine::metadata(const QString& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const QString k = key.toLower();
    if (k == QLatin1String("title"))
        return m_title;
    if (k == QLatin1String("creator") || k == QLatin1String("author"))
        return m_creator;
    return {};
}

QVector<OutlineItem> ChmEngine::outline() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_outline;
}

PageInfo ChmEngine::pageDimensions(int page) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_chm || page < 1 || page > m_htmlPages.size())
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

// /#SYSTEM: DWORD version header, then WORD type, WORD size, payload
// records. Types used here: 1 = contents (.hhc) path, 2 = default topic,
// 3 = title, 4 = locale blob (first DWORD = LCID), 9 = creator.
void ChmEngine::parseSystemData() {
    const QByteArray sys = readEntry(QStringLiteral("/#SYSTEM"));
    if (sys.size() < 4)
        return;

    const unsigned char* p = reinterpret_cast<const unsigned char*>(sys.constData());
    int pos = 4;
    while (pos + 4 <= sys.size()) {
        const quint16 type = le16(p + pos);
        pos += 2;
        const quint16 size = le16(p + pos);
        pos += 2;
        if (pos + size > sys.size())
            break;
        QByteArray payload(sys.constData() + pos, size);
        pos += size;
        while (payload.endsWith('\0'))
            payload.chop(1);

        switch (type) {
            case 1:
                m_systemToc = decodeText(payload);
                break;
            case 2:
                m_systemHome = decodeText(payload);
                break;
            case 3:
                m_title = decodeText(payload);
                break;
            case 4:
                if (payload.size() >= 4)
                    m_codepage = lcidToCodepage(le32(reinterpret_cast<const unsigned char*>(payload.constData())));
                break;
            case 9:
                m_creator = decodeText(payload);
                break;
            default:
                break;
        }
    }
}

// First row of /#WINDOWS carries the main window's toc (+0x60) and home
// (+0x68) offsets into /#STRINGS; used when /#SYSTEM lacks types 1/2.
ChmEngine::WindowsPaths ChmEngine::windowsPaths() const {
    WindowsPaths wp;
    const QByteArray win = readEntry(QStringLiteral("/#WINDOWS"));
    const QByteArray strings = readEntry(QStringLiteral("/#STRINGS"));
    if (win.size() < 8 + 188 || strings.isEmpty())
        return wp;

    const quint32 entrySize =
        le32(reinterpret_cast<const unsigned char*>(win.constData()) + 4);
    if (entrySize < 188 || static_cast<unsigned long long>(win.size()) - 8 < entrySize)
        return wp;

    const unsigned char* row =
        reinterpret_cast<const unsigned char*>(win.constData()) + 8;
    wp.toc = stringAt(strings, le32(row + 0x60));
    wp.home = stringAt(strings, le32(row + 0x68));
    return wp;
}

// Composes reading order and the outline after enumeration and /#SYSTEM
// parsing: [home] + [.hhc topic locals] + [remaining archive entries], then
// resolves .hhc nodes against that list; falls back to the flat /#WINDOWS
// outline when no usable TOC exists.
void ChmEngine::composeDocument() {
    m_outline.clear();

    const WindowsPaths wp = windowsPaths();
    const QString home = !m_systemHome.isEmpty() ? m_systemHome : wp.home;
    const QString tocPath = !m_systemToc.isEmpty() ? m_systemToc : wp.toc;

    QVector<HhcNode> hhc;
    bool haveHhc = false;
    if (!tocPath.isEmpty()) {
        const QByteArray hhcBytes = readEntry(tocPath);
        if (!hhcBytes.isEmpty())
            hhc = parseHhc(decodeText(hhcBytes));
        haveHhc = !hhc.isEmpty();
    }

    QVector<QString> ordered;
    ordered.reserve(m_htmlPages.size());
    QSet<QString> seen;
    seen.reserve(m_htmlPages.size());
    auto push = [&](const QString& p) {
        if (!isHtmlPath(p))
            return;
        const QString n = normalizePath(p);
        if (n.isEmpty() || seen.contains(n))
            return;
        seen.insert(n);
        ordered.append(p);
    };

    push(home);
    if (haveHhc) {
        std::function<void(const QVector<HhcNode>&)> walkLocals =
            [&](const QVector<HhcNode>& nodes) {
                for (const HhcNode& n : nodes) {
                    push(n.local);
                    walkLocals(n.children);
                }
            };
        walkLocals(hhc);
    }
    for (const QString& p : m_htmlPages)
        push(p);
    if (!ordered.isEmpty())
        m_htmlPages = ordered;

    if (haveHhc) {
        std::function<QVector<OutlineItem>(const QVector<HhcNode>&)> convert =
            [&](const QVector<HhcNode>& nodes) -> QVector<OutlineItem> {
            QVector<OutlineItem> out;
            out.reserve(nodes.size());
            for (const HhcNode& n : nodes) {
                OutlineItem item;
                item.title = n.name.isEmpty() ? n.local : n.name;
                item.pageNo = pageIndexFor(n.local);
                item.resolved = !n.local.isEmpty() && pageIndexOf(n.local) >= 0;
                item.children = convert(n.children);
                out.append(item);
            }
            return out;
        };
        m_outline = convert(hhc);
    }
    if (m_outline.isEmpty())
        m_outline = parseWindowsOutline();
}

// v1 fallback outline: one item per /#WINDOWS row. Title from +0x14 and
// default-topic path from +0x68, both offsets into /#STRINGS. Rows shorter
// than 188 bytes or tables that overflow the file -> empty outline.
QVector<OutlineItem> ChmEngine::parseWindowsOutline() const {
    QVector<OutlineItem> items;
    const QByteArray win = readEntry(QStringLiteral("/#WINDOWS"));
    const QByteArray strings = readEntry(QStringLiteral("/#STRINGS"));
    if (win.size() < 8 || strings.isEmpty())
        return items;

    quint32 count = le32(reinterpret_cast<const unsigned char*>(win.constData()));
    quint32 entrySize =
        le32(reinterpret_cast<const unsigned char*>(win.constData()) + 4);
    if (count == 0 || entrySize < 188)
        return items;

    const unsigned long long avail = static_cast<unsigned long long>(win.size()) - 8;
    if (static_cast<unsigned long long>(count) * entrySize > avail)
        count = static_cast<quint32>(avail / entrySize);

    items.reserve(static_cast<int>(count));
    const unsigned char* base = reinterpret_cast<const unsigned char*>(win.constData()) + 8;
    for (quint32 i = 0; i < count; ++i) {
        const unsigned char* row = base + static_cast<size_t>(i) * entrySize;

        const QString title = stringAt(strings, le32(row + 0x14));
        const QString home = stringAt(strings, le32(row + 0x68));
        OutlineItem item;
        item.title = title.isEmpty() ? home : title;
        if (item.title.isEmpty())
            continue;
        item.pageNo = pageIndexFor(home);
        items.append(item);
    }
    return items;
}

QString ChmEngine::stringAt(const QByteArray& blob, unsigned offset) const {
    if (offset >= static_cast<unsigned>(blob.size()))
        return {};
    int end = static_cast<int>(blob.indexOf('\0', static_cast<int>(offset)));
    if (end < 0)
        end = blob.size();
    QByteArray raw(blob.constData() + offset, end - static_cast<int>(offset));
    return decodeText(raw);
}

int ChmEngine::pageIndexOf(const QString& path) const {
    const QString needle = normalizePath(path);
    if (needle.isEmpty())
        return -1;
    for (int i = 0; i < m_htmlPages.size(); ++i) {
        if (normalizePath(m_htmlPages.at(i)) == needle)
            return i;
    }
    return -1;
}

int ChmEngine::pageIndexFor(const QString& path) const {
    const int idx = pageIndexOf(path);
    return idx >= 0 ? idx + 1 : 1;
}

QString ChmEngine::decodeText(const QByteArray& bytes) const {
    if (bytes.isEmpty())
        return {};
#ifdef _WIN32
    if (m_codepage == 65001 || m_codepage == 20127)
        return QString::fromUtf8(bytes);
    const int wlen = MultiByteToWideChar(static_cast<UINT>(m_codepage), 0,
                                         bytes.constData(), bytes.size(), nullptr, 0);
    if (wlen <= 0)
        return QString::fromLatin1(bytes);
    QString out;
    out.resize(wlen);
    MultiByteToWideChar(static_cast<UINT>(m_codepage), 0, bytes.constData(),
                        bytes.size(), reinterpret_cast<wchar_t*>(out.data()), wlen);
    while (out.endsWith(QChar(u'\0')))
        out.chop(1);
    return out;
#else
    if (m_codepage == 65001 || m_codepage == 20127)
        return QString::fromUtf8(bytes);
    return QString::fromLatin1(bytes);
#endif
}
