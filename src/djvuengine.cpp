#include "djvuengine.h"

#include <QImage>

DjVuEngine::DjVuEngine() = default;

DjVuEngine::~DjVuEngine() {
    close();
}

static bool waitForDocInfo(ddjvu_context_t* ctx, ddjvu_document_t* doc) {
    while (!ddjvu_document_decoding_done(doc)) {
        ddjvu_message_wait(ctx);
        ddjvu_message_t* msg;
        while ((msg = ddjvu_message_pop(ctx))) {
            if (msg->any.what == DDJVU_DOCINFO)
                return true;
            if (msg->any.what == DDJVU_FAILED)
                return false;
        }
    }
    return true;
}

bool DjVuEngine::open(const QString& path) {
    close();

    m_ctx = ddjvu_context_create("wlx-multidoc-viewer");
    if (!m_ctx)
        return false;

    QByteArray pathBytes = path.toLocal8Bit();
    m_doc = ddjvu_document_create_by_filename(m_ctx, pathBytes.constData());
    if (!m_doc) {
        ddjvu_context_release(m_ctx);
        m_ctx = nullptr;
        return false;
    }

    if (!waitForDocInfo(m_ctx, m_doc)) {
        ddjvu_document_release(m_doc);
        ddjvu_context_release(m_ctx);
        m_doc = nullptr;
        m_ctx = nullptr;
        return false;
    }

    m_pageCount = ddjvu_document_get_page_num(m_doc);
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
    m_pageCount = 0;
    m_path.clear();
}

bool DjVuEngine::isOpen() const {
    return m_doc != nullptr;
}

int DjVuEngine::pageCount() const {
    return m_pageCount;
}

QImage DjVuEngine::renderPage(int page, float zoom) {
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    ddjvu_page_t* djpage = ddjvu_page_create_by_pageno(m_doc, page - 1);
    if (!djpage)
        return {};

    while (!ddjvu_page_decoding_done(djpage))
        ddjvu_message_wait(m_ctx);

    int w = 0, h = 0;
    ddjvu_page_get_rendered_size(djpage, &w, &h);

    if (w <= 0 || h <= 0) {
        ddjvu_page_release(djpage);
        return {};
    }

    int scaledW = static_cast<int>(w * zoom);
    int scaledH = static_cast<int>(h * zoom);
    if (scaledW <= 0 || scaledH <= 0) {
        ddjvu_page_release(djpage);
        return {};
    }

    int stride = scaledW * 4;
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
    pageRect.w = w;
    pageRect.h = h;

    ddjvu_page_render(djpage, DDJVU_RENDER_COLOR,
                      &pageRect, &rider, m_ctx, DDJVU_FORMAT_BGR24,
                      stride, buffer);

    QImage img(buffer, scaledW, scaledH, stride, QImage::Format_RGB888);

    // DjVuLibre renders bottom-up; flip vertically.
    QImage flipped = img.mirrored(false, true);

    delete[] buffer;
    ddjvu_page_release(djpage);

    return flipped;
}

QString DjVuEngine::extractText(int page) {
    if (!m_ctx || !m_doc || page < 1 || page > m_pageCount)
        return {};

    ddjvu_page_t* djpage = ddjvu_page_create_by_pageno(m_doc, page - 1);
    if (!djpage)
        return {};

    while (!ddjvu_page_decoding_done(djpage))
        ddjvu_message_wait(m_ctx);

    char* text = ddjvu_page_text(djpage);
    QString result = text ? QString::fromUtf8(text) : QString();
    free(text);

    ddjvu_page_release(djpage);
    return result;
}

QString DjVuEngine::metadata(const QString& key) const {
    Q_UNUSED(key)
    // DjVu format has limited metadata support.
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

    int w = 0, h = 0;
    ddjvu_page_get_rendered_size(djpage, &w, &h);

    ddjvu_page_release(djpage);
    return {w, h};
}
