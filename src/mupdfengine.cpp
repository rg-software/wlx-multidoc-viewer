#include "mupdfengine.h"

#include <mupdf/pdf.h>
#include <QImage>
#include <QTransform>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

// Big-endian readers, per PalmDB/MOBI byte order.
uint16_t beU16(const unsigned char* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t beU32(const unsigned char* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

// Bounding rect of one fz_quad in page space (y-down, page dimensions from
// fz_bound_page), matching the convention used for word bboxes elsewhere.
QRectF quadRect(const fz_quad& q) {
    return QRectF(QPointF(q.ul.x, q.ul.y), QPointF(q.lr.x, q.lr.y)).normalized();
}

// One page-space rect carrying its reading-order line index (for spacing).
struct SearchGlyph {
    QChar c;
    QRectF box;
    int line = 0;
};

} // namespace

MuPdfEngine::MuPdfEngine() = default;

MuPdfEngine::~MuPdfEngine() {
    close();
}

bool MuPdfEngine::open(const QString& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    dropDocument();

    if (!QFile::exists(path)) {
        qWarning() << "MuPdfEngine: file does not exist:" << path;
        return false;
    }

    m_ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!m_ctx) {
        qWarning() << "MuPdfEngine: failed to create MuPDF context for" << path;
        return false;
    }

    fz_register_document_handlers(m_ctx);

    // Open via a FILE stream so names with CJK/Cyrillic work regardless of the
    // ANSI code page: on Windows use the wide-char API; elsewhere UTF-8.
    const QByteArray magic = path.toUtf8();
    const QString suffix = QFileInfo(path).suffix().toLower();
    const bool isMobi = (suffix == "mobi" || suffix == "prc");

    fz_stream* stm = nullptr;
    bool opened = false;
    fz_try(m_ctx) {
#ifdef _WIN32
        stm = fz_open_file_w(m_ctx, reinterpret_cast<const wchar_t*>(path.utf16()));
#else
        const QByteArray utf8 = path.toUtf8();
        stm = fz_open_file(m_ctx, utf8.constData());
#endif
        if (isMobi) {
            // Read the EXTH cover record only; the document itself is still
            // opened via the plain path below so muPDF's own section
            // page-breaks (mbp:pagebreak) are preserved byte-for-byte.
            m_bodyHasCover = tryOpenMobi(stm);
            fz_seek(m_ctx, stm, 0, SEEK_SET);
        }
        // Pass the path as the "magic" hint so format detection uses the
        // file extension (like fz_open_document did) instead of sniffing.
        m_doc = fz_open_document_with_stream(m_ctx, magic.constData(), stm);
        opened = (m_doc != nullptr);
    }
    fz_always(m_ctx) {
        // The document keeps its own stream reference; drop ours either way.
        if (stm)
            fz_drop_stream(m_ctx, stm);
    }
    fz_catch(m_ctx) {
        qWarning() << "MuPdfEngine: fz_open_document failed for" << path;
        fz_drop_context(m_ctx);
        m_ctx = nullptr;
        m_doc = nullptr;
        m_bodyHasCover = false;
        m_coverImage = QImage();
        return false;
    }

    fz_try(m_ctx) {
        m_pageCount = fz_count_pages(m_ctx, m_doc);
    }
    fz_catch(m_ctx) {
        qWarning() << "MuPdfEngine: fz_count_pages failed for" << path;
        m_pageCount = 0;
    }

    if (m_bodyHasCover && m_pageCount > 0)
        ++m_pageCount; // public page 1 is the synthetic cover

    return m_doc != nullptr;
}

bool MuPdfEngine::tryOpenMobi(fz_stream* file) {
    fz_buffer* mobi = nullptr;
    bool ok = false;

    fz_var(mobi);
    fz_var(ok);

    fz_try(m_ctx) {
        mobi = fz_read_all(m_ctx, file, 0);
        unsigned char* d = mobi->data;
        const size_t len = mobi->len;

        // PalmDB header: 32 (name) + 28 (attributes) + 8 (type/creator) +
        // 8 (internal) = 76, then uint16 record count, then record info
        // entries of {uint32 data offset, uint32 attrs/uid} each.
        if (len < 78 + 8)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: file too small");
        if (memcmp(d + 60, "BOOKMOBI", 8) != 0 && memcmp(d + 60, "TEXtREAd", 8) != 0)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: bad type/creator");
        const uint16_t nrec = beU16(d + 76);
        if (nrec < 1 || 78 + static_cast<size_t>(nrec) * 8 > len)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: bad record table");

        // Record offsets are strictly increasing and point inside the file
        // (mobi.c's invariants); append file size as a sentinel.
        const size_t dataStart = 78 + static_cast<size_t>(nrec) * 8;
        std::vector<size_t> recOff;
        recOff.reserve(nrec + 1);
        size_t prevOff = 0;
        for (uint16_t i = 0; i < nrec; ++i) {
            const size_t off = beU32(d + 78 + static_cast<size_t>(i) * 8);
            if (off < dataStart || off >= len || (i > 0 && off <= prevOff))
                fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: record offset out of range");
            prevOff = off;
            recOff.push_back(off);
        }
        recOff.push_back(len);

        // Record 0 is the MOBI header: 16-byte PalmDOC prefix, then "MOBI",
        // then the header. "First Image Index" lives at header offset 0x5C,
        // EXTH presence flag (bit 0x40) at offset 0x70, the EXTH block right
        // after the header (at 16 + headerLength).
        const size_t hdr = recOff[0] + 16;
        if (hdr + 4 > len || memcmp(d + recOff[0] + 16, "MOBI", 4) != 0)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: no MOBI magic");
        const uint32_t hdrLen = beU32(d + hdr + 4);
        if (hdrLen < 0x74 || hdr + hdrLen + 12 > len)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: header too short");
        if (!(beU32(d + hdr + 0x70) & 0x40) || memcmp(d + hdr + hdrLen, "EXTH", 4) != 0)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: no EXTH block");
        const uint32_t exthCount = beU32(d + hdr + hdrLen + 8);

        // Walk EXTH records {uint32 type, uint32 size, data}; type 201 is the
        // cover record index relative to the first image record. The size field
        // INCLUDES the 8-byte type/size header, so the cursor advances by size.
        int64_t coverIndex = -1;
        size_t p = hdr + hdrLen + 12;
        for (uint32_t i = 0; i < exthCount && coverIndex < 0; ++i) {
            if (p + 8 > len)
                break;
            const uint32_t type = beU32(d + p);
            const uint32_t size = beU32(d + p + 4);
            if (p + 8 + size > len)
                break;
            if (type == 201 && size >= 4) {
                const int32_t rel = static_cast<int32_t>(beU32(d + p + 8));
                const int64_t idx = static_cast<int64_t>(beU32(d + hdr + 0x5C)) + rel;
                if (idx >= 0 && idx < nrec)
                    coverIndex = idx;
            }
            p += size;
        }
        if (coverIndex < 0)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: cover record index missing");

        const size_t cover = recOff[static_cast<size_t>(coverIndex)];
        const size_t coverSize = recOff[static_cast<size_t>(coverIndex) + 1] - cover;
        if (coverSize < 8 || cover + coverSize > len)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: cover record out of range");
        if (fz_recognize_image_format(m_ctx, d + cover) == FZ_IMAGE_UNKNOWN)
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: cover record is not an image");

        // Decode the cover via Qt (same codec path the comic engine uses).
        const QImage coverImg = QImage::fromData(
            reinterpret_cast<const uchar*>(d + cover), static_cast<qsizetype>(coverSize));
        if (coverImg.isNull())
            fz_throw(m_ctx, FZ_ERROR_FORMAT, "MOBI: cover record undecodable");

        m_coverImage = coverImg;
        m_bodyHasCover = true;
        ok = true;
    }
    fz_always(m_ctx) {
        if (mobi)
            fz_drop_buffer(m_ctx, mobi);
    }
    fz_catch(m_ctx) {
        // Swallow cover-extraction failures (wrong structure, no cover, bad
        // image): the engine opens the body via the plain MuPDF path unchanged.
        // Only fatal system errors (incl. out-of-memory) propagate.
        if (fz_caught(m_ctx) == FZ_ERROR_SYSTEM)
            fz_rethrow(m_ctx);
        m_bodyHasCover = false;
        m_coverImage = QImage();
        ok = false;
    }

    return ok;
}

void MuPdfEngine::dropDocument() {
    if (m_doc) {
        fz_drop_document(m_ctx, m_doc);
        m_doc = nullptr;
    }
    if (m_ctx) {
        fz_drop_context(m_ctx);
        m_ctx = nullptr;
    }
    m_pageCount = 0;
    m_bodyHasCover = false;
    m_coverImage = QImage();
}

void MuPdfEngine::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    dropDocument();
}

bool MuPdfEngine::isOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_doc != nullptr;
}

int MuPdfEngine::pageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pageCount;
}

QImage MuPdfEngine::renderPage(int page, float zoom, float dpiScale, int rotation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    // Synthetic cover page: render the stored cover image, scaled so it fills
    // the same pixel area a body page would occupy at this zoom (point 1:
    // size matches the body text area). Rotation is applied around the center
    // like the body render path.
    if (m_bodyHasCover && page == 1) {
        QImage base = m_coverImage;
        if (base.isNull())
            return {};

        int bodyW = 1, bodyH = 1;
        const PageInfo body = (m_pageCount > 1) ? pageDimensionsRaw(1) : PageInfo{};
        if (body.width > 0 && body.height > 0) {
            bodyW = body.width;
            bodyH = body.height;
        }
        const int tw = qMax(1, qRound(bodyW * zoom * dpiScale));
        const int th = qMax(1, qRound(bodyH * zoom * dpiScale));

        // Fit the cover inside the body content area, not the full page:
        // MuPDF's html layout applies @page{margin:3em 2em} at the default
        // em (11pt) -> 33pt top/bottom and 22pt left/right, so body text sits
        // inside those margins. The cover is drawn to that same content box,
        // centered, leaving the page margins visible around it (point 1).
        const double marginH = 3.0 * 11.0; // 3em @ 11pt
        const double marginW = 2.0 * 11.0; // 2em @ 11pt
        const double cw = qMax(1.0, bodyW - 2.0 * marginW);
        const double ch = qMax(1.0, bodyH - 2.0 * marginH);
        const double scale = qMin(cw / base.width(), ch / base.height());
        const int dw = qMax(1, qRound(base.width() * scale));
        const int dh = qMax(1, qRound(base.height() * scale));
        QImage cover = base.scaled(dw, dh, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        QImage canvas(tw, th, QImage::Format_RGB888);
        canvas.fill(Qt::white);
        const int ox = (tw - dw) / 2;
        const int oy = (th - dh) / 2;
        QImage src = cover.convertToFormat(QImage::Format_RGB888);
        for (int y = 0; y < dh; ++y) {
            if (oy + y < 0 || oy + y >= th)
                continue;
            std::memcpy(canvas.scanLine(oy + y) + ox * 3, src.scanLine(y),
                        static_cast<size_t>(dw) * 3);
        }

        if (rotation) {
            QTransform t;
            t.translate(tw / 2.0, th / 2.0);
            t.rotate(rotation);
            t.translate(-tw / 2.0, -th / 2.0);
            canvas = canvas.transformed(t, Qt::SmoothTransformation);
        }
        return canvas;
    }

    fz_page* fzpage = nullptr;
    fz_pixmap* pixmap = nullptr;
    QImage result;

    float effectiveZoom = zoom * dpiScale;

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, bodyPageIndex(page));

        fz_rect bounds = fz_bound_page(m_ctx, fzpage);
        fz_matrix ctm = fz_scale(effectiveZoom, effectiveZoom);
        if (rotation) {
            float cx = (bounds.x0 + bounds.x1) * 0.5f;
            float cy = (bounds.y0 + bounds.y1) * 0.5f;
            ctm = fz_concat(ctm, fz_translate(cx, cy));
            ctm = fz_concat(ctm, fz_rotate(static_cast<float>(rotation)));
            ctm = fz_concat(ctm, fz_translate(-cx, -cy));
        }

        pixmap = fz_new_pixmap_from_page(m_ctx, fzpage, ctm, fz_device_rgb(m_ctx), 0);

        if (pixmap && pixmap->w > 0 && pixmap->h > 0) {
            int w = pixmap->w;
            int h = pixmap->h;
            int stride = w * 3;

            QImage img(pixmap->samples, w, h, stride, QImage::Format_RGB888);
            result = img.copy();
        }
    }
    fz_always(m_ctx) {
        if (pixmap)
            fz_drop_pixmap(m_ctx, pixmap);
        if (fzpage)
            fz_drop_page(m_ctx, fzpage);
    }
    fz_catch(m_ctx) {
        return {};
    }

    return result;
}

PageText MuPdfEngine::pageText(int page) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    // The synthetic cover page carries no selectable text.
    if (m_bodyHasCover && page == 1)
        return {};

    fz_page* fzpage = nullptr;
    fz_stext_page* stext = nullptr;
    PageText result;

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, bodyPageIndex(page));

        fz_stext_options opts;
        opts.flags = FZ_STEXT_ACCURATE_BBOXES;
        stext = fz_new_stext_page_from_page(m_ctx, fzpage, &opts);

        int lineIndex = 0;
        for (fz_stext_block* block = stext->first_block; block; block = block->next) {
            if (block->type != FZ_STEXT_BLOCK_TEXT)
                continue;
            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                // Group characters into words on whitespace, unioning each
                // char's quad into the current word's bbox.
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
                    const fz_quad q = ch->quad;
                    // MuPDF quads are in page space (y-down), ul/lr opposite corners.
                    const QRectF r(QPointF(q.ul.x, q.ul.y), QPointF(q.lr.x, q.lr.y));
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
    fz_always(m_ctx) {
        if (stext)
            fz_drop_stext_page(m_ctx, stext);
        if (fzpage)
            fz_drop_page(m_ctx, fzpage);
    }
    fz_catch(m_ctx) {
        return {};
    }

    return result;
}

QString MuPdfEngine::extractText(int page) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    fz_page* fzpage = nullptr;
    fz_stext_page* stext = nullptr;
    QString result;

    // The synthetic cover page carries no text.
    if (m_bodyHasCover && page == 1)
        return {};

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, bodyPageIndex(page));

        fz_stext_options opts;
        opts.flags = FZ_STEXT_ACCURATE_BBOXES;
        stext = fz_new_stext_page_from_page(m_ctx, fzpage, &opts);

        QByteArray textBuf;
        for (fz_stext_block* block = stext->first_block; block; block = block->next) {
            if (block->type != FZ_STEXT_BLOCK_TEXT)
                continue;
            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                    char buf[8];
                    int n = fz_runetochar(buf, ch->c);
                    textBuf.append(buf, n);
                }
                textBuf.append('\n');
            }
        }
        result = QString::fromUtf8(textBuf);
    }
    fz_always(m_ctx) {
        if (stext)
            fz_drop_stext_page(m_ctx, stext);
        if (fzpage)
            fz_drop_page(m_ctx, fzpage);
    }
    fz_catch(m_ctx) {
        return {};
    }

    return result;
}

QString MuPdfEngine::metadata(const QString& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc)
        return {};

    QByteArray keyUtf8 = key.toUtf8();
    char value[256] = {};

    fz_try(m_ctx) {
        int len = fz_lookup_metadata(m_ctx, m_doc, keyUtf8.constData(), value, sizeof(value));
        if (len > 0)
            return QString::fromUtf8(value);
    }
    fz_catch(m_ctx) {
    }

    return {};
}

QVector<OutlineItem> MuPdfEngine::outline() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc)
        return {};

    QVector<OutlineItem> items;
    fz_outline* root = nullptr;

    fz_try(m_ctx) {
        root = fz_load_outline(m_ctx, m_doc);
    }
    fz_catch(m_ctx) {
        return {};
    }

    if (!root)
        return {};

    std::function<void(fz_outline*, QVector<OutlineItem>&)> walk =
        [&](fz_outline* node, QVector<OutlineItem>& out) {
            while (node) {
                OutlineItem item;
                item.title = QString::fromUtf8(node->title);
                // MuPDF pages are 1-based; shift by the synthetic cover if
                // any so outline targets materialize on the same public page.
                item.pageNo = node->page.page + 1 + (m_bodyHasCover ? 1 : 0);
                if (node->down)
                    walk(node->down, item.children);
                out.append(item);
                node = node->next;
            }
        };

    walk(root, items);
    fz_drop_outline(m_ctx, root);

    return items;
}

// Assumes the caller holds m_mutex.
PageInfo MuPdfEngine::pageDimensionsRaw(int bodyPage) const {
    if (!m_ctx || !m_doc || bodyPage < 1)
        return {};

    fz_page* fzpage = nullptr;
    PageInfo info;

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, bodyPage - 1);
        fz_rect bounds = fz_bound_page(m_ctx, fzpage);
        info.width = static_cast<int>(bounds.x1 - bounds.x0);
        info.height = static_cast<int>(bounds.y1 - bounds.y0);
    }
    fz_always(m_ctx) {
        if (fzpage)
            fz_drop_page(m_ctx, fzpage);
    }
    fz_catch(m_ctx) {
        return {};
    }

    return info;
}

PageInfo MuPdfEngine::pageDimensions(int page) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    // Synthetic cover page shares the body page size.
    if (m_bodyHasCover && page == 1)
        return pageDimensionsRaw(1);

    return pageDimensionsRaw(bodyPageIndex(page) + 1);
}

QVector<TextMatch> MuPdfEngine::searchText(int page, const QString& needle, bool matchCase) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount || needle.isEmpty())
        return {};

    // The synthetic cover page has no searchable text.
    if (m_bodyHasCover && page == 1)
        return {};

    fz_page* fzpage = nullptr;
    fz_stext_page* stext = nullptr;
    float pageWidth = 0.0f;
    float pageHeight = 0.0f;

    // Normalized match rects for this page (filled inside fz_try).
    QVector<QRectF> rects;

    // Search always walks the structured-text glyphs ourselves so case
    // sensitivity is exactly what the toolbar requests (Qt::CaseSensitive /
    // Qt::CaseInsensitive). MuPDF's built-in fz_search only does
    // case-insensitive and has extra normalization quirks, so it is not used;
    // this keeps "match case off" / "match case on" deterministic.
    const Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const QString needleNorm = matchCase ? needle : needle.toLower();

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, bodyPageIndex(page));

        const fz_rect bounds = fz_bound_page(m_ctx, fzpage);
        pageWidth = bounds.x1 - bounds.x0;
        pageHeight = bounds.y1 - bounds.y0;
        if (pageWidth < 1.0f || pageHeight < 1.0f) {
            // Page too small to hold coordinates; no hits are possible.
        } else {
            fz_stext_options opts;
            opts.flags = FZ_STEXT_ACCURATE_BBOXES;
            stext = fz_new_stext_page_from_page(m_ctx, fzpage, &opts);

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
                        glyphs.append(SearchGlyph{c, quadRect(ch->quad).normalized(), lineNo});
                    }
                    ++lineNo;
                }
            }

            if (!glyphs.isEmpty()) {
                // Flatten glyphs to a string: a space is inserted between
                // glyphs from different reading-order lines so multi-word
                // terms match; separators carry no geometry (index -1).
                QString run;
                QVector<int> runGlyph; // glyph index or -1 for a separator
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

                // Substring search over the run honoring the case mode; union
                // each match's glyph rects into one normalized page-space rect.
                int from = 0;
                while (from <= run.size()) {
                    const int pos = runNorm.indexOf(needleNorm, from, cs);
                    if (pos < 0)
                        break;
                    QRectF box;
                    const int last = pos + needle.size();
                    for (int i = pos; i < last; ++i) {
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
                    from = pos + 1;
                }
            }
        }
    }
    fz_always(m_ctx) {
        if (stext)
            fz_drop_stext_page(m_ctx, stext);
        if (fzpage)
            fz_drop_page(m_ctx, fzpage);
    }
    fz_catch(m_ctx) {
        return {};
    }

    if (rects.isEmpty())
        return {};
    TextMatch match;
    match.page = page;
    match.rects = std::move(rects);
    return {match};
}
