#include "mupdfengine.h"

#include <mupdf/pdf.h>
#include <QImage>
#include <QFile>
#include <QDebug>

namespace {

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
    fz_stream* stm = nullptr;
    bool opened = false;
    fz_try(m_ctx) {
#ifdef _WIN32
        stm = fz_open_file_w(m_ctx, reinterpret_cast<const wchar_t*>(path.utf16()));
#else
        const QByteArray utf8 = path.toUtf8();
        stm = fz_open_file(m_ctx, utf8.constData());
#endif
        // Pass the path as the "magic" hint so format detection uses the file
        // extension (like fz_open_document did) instead of sniffing only.
        const QByteArray magic = path.toUtf8();
        m_doc = fz_open_document_with_stream(m_ctx, magic.constData(), stm);
        opened = (m_doc != nullptr);
    }
    fz_always(m_ctx) {
        // The document keeps its own stream reference; drop ours on success.
        if (opened && stm)
            fz_drop_stream(m_ctx, stm);
    }
    fz_catch(m_ctx) {
        qWarning() << "MuPdfEngine: fz_open_document failed for" << path;
        fz_drop_context(m_ctx);
        m_ctx = nullptr;
        m_doc = nullptr;
        return false;
    }

    fz_try(m_ctx) {
        m_pageCount = fz_count_pages(m_ctx, m_doc);
    }
    fz_catch(m_ctx) {
        qWarning() << "MuPdfEngine: fz_count_pages failed for" << path;
        m_pageCount = 0;
    }

    return m_doc != nullptr;
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

    fz_page* fzpage = nullptr;
    fz_pixmap* pixmap = nullptr;
    QImage result;

    float effectiveZoom = zoom * dpiScale;

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, page - 1);

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

    fz_page* fzpage = nullptr;
    fz_stext_page* stext = nullptr;
    PageText result;

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, page - 1);

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

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, page - 1);

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
                item.pageNo = node->page.page + 1;
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

PageInfo MuPdfEngine::pageDimensions(int page) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    fz_page* fzpage = nullptr;
    PageInfo info;

    fz_try(m_ctx) {
        fzpage = fz_load_page(m_ctx, m_doc, page - 1);
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

QVector<TextMatch> MuPdfEngine::searchText(int page, const QString& needle, bool matchCase) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount || needle.isEmpty())
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
        fzpage = fz_load_page(m_ctx, m_doc, page - 1);

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
