#include "djvuengine.h"

#include <QImage>
#include <QDebug>
#include <cstring>

DjVuEngine::DjVuEngine() = default;

DjVuEngine::~DjVuEngine() {
    close();
}

static bool waitForDocInfo(ddjvu_context_t* ctx, ddjvu_document_t* doc) {
    while (!ddjvu_document_decoding_done(doc)) {
        ddjvu_message_wait(ctx);
        const ddjvu_message_t* msg;
        while ((msg = ddjvu_message_peek(ctx))) {
            bool done = false;
            if (msg->m_any.tag == DDJVU_DOCINFO) {
                ddjvu_message_pop(ctx);
                return true;
            }
            if (msg->m_any.tag == DDJVU_ERROR) {
                ddjvu_message_pop(ctx);
                return false;
            }
            ddjvu_message_pop(ctx);
        }
    }
    return true;
}

bool DjVuEngine::open(const QString& path) {
    close();

    m_ctx = ddjvu_context_create("wlx-multidoc-viewer");
    if (!m_ctx) {
        qWarning() << "DjVuEngine: failed to create context for" << path;
        return false;
    }

    QByteArray pathBytes = path.toLocal8Bit();
    m_doc = ddjvu_document_create_by_filename(m_ctx, pathBytes.constData(), 1);
    if (!m_doc) {
        qWarning() << "DjVuEngine: ddjvu_document_create_by_filename failed for" << path;
        ddjvu_context_release(m_ctx);
        m_ctx = nullptr;
        return false;
    }

    if (!waitForDocInfo(m_ctx, m_doc)) {
        qWarning() << "DjVuEngine: waitForDocInfo failed for" << path;
        ddjvu_document_release(m_doc);
        ddjvu_context_release(m_ctx);
        m_doc = nullptr;
        m_ctx = nullptr;
        return false;
    }

    m_pageCount = ddjvu_document_get_pagenum(m_doc);
    m_path = path;
    return true;
}

void DjVuEngine::close() {
    if (m_doc) {
        ddjvu_document_release(m_doc);
        m_doc = nullptr;
    }
    if (m_ctx) {
        ddjvu_context_release(m_ctx);
        m_ctx = nullptr;
    }
    if (m_fmt) {
        ddjvu_format_release(m_fmt);
        m_fmt = nullptr;
    }
    m_pageCount = 0;
    m_path.clear();
}

bool DjVuEngine::isOpen() const {
    return m_doc != nullptr;
}

int DjVuEngine::pageCount() const {
    return m_pageCount;
}

QImage DjVuEngine::renderPage(int page, float zoom, float dpiScale, int rotation) {
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    ddjvu_page_t* djpage = ddjvu_page_create_by_pageno(m_doc, page - 1);
    if (!djpage)
        return {};

    while (!ddjvu_page_decoding_done(djpage))
        ddjvu_message_wait(m_ctx);

    int w = ddjvu_page_get_width(djpage);
    int h = ddjvu_page_get_height(djpage);

    if (w <= 0 || h <= 0) {
        ddjvu_page_release(djpage);
        return {};
    }

    float effectiveZoom = zoom * dpiScale;
    int scaledW = static_cast<int>(w * effectiveZoom);
    int scaledH = static_cast<int>(h * effectiveZoom);
    if (rotation == 90 || rotation == 270)
        std::swap(scaledW, scaledH);
    if (scaledW <= 0 || scaledH <= 0) {
        ddjvu_page_release(djpage);
        return {};
    }

    if (!m_fmt) {
        m_fmt = ddjvu_format_create(DDJVU_FORMAT_RGB24, 0, nullptr);
        if (!m_fmt) {
            ddjvu_page_release(djpage);
            return {};
        }
        ddjvu_format_set_row_order(m_fmt, 1);
    }

    int stride = scaledW * 3;
    unsigned char* buffer = new (std::nothrow) unsigned char[stride * scaledH];
    if (!buffer) {
        ddjvu_page_release(djpage);
        return {};
    }

    memset(buffer, 0, stride * scaledH);

    ddjvu_rect_t rider;
    rider.x = 0;
    rider.y = 0;
    rider.w = scaledW;
    rider.h = scaledH;

    ddjvu_rect_t pageRect;
    pageRect.x = 0;
    pageRect.y = 0;
    pageRect.w = scaledW;
    pageRect.h = scaledH;

    ddjvu_page_render(djpage, DDJVU_RENDER_COLOR,
                      &pageRect, &rider, m_fmt,
                      stride, reinterpret_cast<char*>(buffer));

    QImage img(buffer, scaledW, scaledH, stride, QImage::Format_RGB888);
    QImage result = img.copy();

    if (rotation != 0) {
        QTransform t;
        t.rotate(rotation);
        result = result.transformed(t, Qt::SmoothTransformation);
    }

    delete[] buffer;
    ddjvu_page_release(djpage);

    return result;
}

// NOTE: DjVu text-layer extraction is intentionally not implemented. The
// vcpkg ddjvulc static build does not export the core miniexp accessors
// (miniexp_car/miniexp_cdr/miniexp_consp/miniexp_symbolp/miniexp_to_int are
// module-local `Static` symbols and cannot be linked), so `ddjvu_document_get_pagetext`
// trees cannot be walked. Selection therefore works for MuPDF-backed formats;
// DjVu pages report no text layer (empty). Revisit if a djvulibre build exports
// the miniexp public API.
PageText DjVuEngine::pageText(int page) {
    Q_UNUSED(page)
    return {};
}

QString DjVuEngine::extractText(int page) {
    Q_UNUSED(page)
    return {};
}

QString DjVuEngine::metadata(const QString& key) const {
    Q_UNUSED(key)
    return {};
}

QVector<OutlineItem> DjVuEngine::outline() const {
    return {};
}

PageInfo DjVuEngine::pageDimensions(int page) const {
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    ddjvu_page_t* djpage = ddjvu_page_create_by_pageno(m_doc, page - 1);
    if (!djpage)
        return {};

    while (!ddjvu_page_decoding_done(djpage))
        ddjvu_message_wait(m_ctx);

    int w = ddjvu_page_get_width(djpage);
    int h = ddjvu_page_get_height(djpage);

    ddjvu_page_release(djpage);
    return {w, h};
}
