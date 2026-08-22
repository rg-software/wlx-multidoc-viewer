#include "mupdfengine.h"

#include <mupdf/pdf.h>
#include <QImage>
#include <QFile>
#include <QDebug>

MuPdfEngine::MuPdfEngine() = default;

MuPdfEngine::~MuPdfEngine() {
    close();
}

bool MuPdfEngine::open(const QString& path) {
    close();

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

    QByteArray pathBytes = path.toUtf8();
    fz_try(m_ctx) {
        m_doc = fz_open_document(m_ctx, pathBytes.constData());
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

void MuPdfEngine::close() {
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

bool MuPdfEngine::isOpen() const {
    return m_doc != nullptr;
}

int MuPdfEngine::pageCount() const {
    return m_pageCount;
}

QImage MuPdfEngine::renderPage(int page, float zoom, float dpiScale, int rotation) {
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
