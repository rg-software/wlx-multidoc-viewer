// Design notes (SumatraPDF reference patterns; read-only, not copied)
// ---------------------------------------------------
// Continuous-mode page strip layout: SumatraPDF renders all currently-visible
// pages into a single tall bitmap and lets the scrollbar walk through it. We
// do the same in renderVisiblePages(), with the strip height capped at
// kMaxStripHeight pixels to bound memory on very long documents (see AGENTS.md).
//
// Paged<->continuous transition: SumatraPDF anchors the scroll position so
// the page that was at the top of the viewport stays at the top. We do the
// same via toggleMode(); the controller tracks only the current page index,
// while each viewer owns viewport anchoring (scroll offset capture/restore)
// around continuous-mode navigation.
//
// Wheel-event handling is viewer-specific: paged mode advances one page per
// notch, continuous mode scrolls smoothly (owned entirely by the viewers).
//
// Engine abstraction: SumatraPDF's EngineBase shows the value of one render
// method that takes (page, transform, target_size). Our DocumentEngine stays
// as (page, zoom, dpiScale, rotation) per the existing project layout.
// ---------------------------------------------------

#include "viewercontroller.h"
#include "viewer_settings.h"

#include <algorithm>

#include <QDebug>
#include <QPainter>

namespace {
using viewer_settings::kPageGap;
}

ViewerController::ViewerController() = default;

void ViewerController::setEngine(std::unique_ptr<DocumentEngine> engine) {
    m_engine = std::move(engine);
}

bool ViewerController::openDocument(const QString& path) {
    if (!m_engine) {
        qWarning() << "ViewerController: no engine set, cannot open" << path;
        return false;
    }
    if (!m_engine->open(path)) {
        qWarning() << "ViewerController: engine failed to open" << path;
        return false;
    }
    m_state.setPageCount(m_engine->pageCount());
    m_state.resetPage();
    m_fitMode = FitMode::FitToPage;
    m_rotation = 0;
    recomputeFitZoom();
    notifyChanged();
    return true;
}

void ViewerController::closeDocument() {
    if (m_engine)
        m_engine->close();
    m_state = ViewerState();
    m_fitMode = FitMode::FitToPage;
    m_rotation = 0;
    notifyChanged();
}

bool ViewerController::nextPage() {
    if (!isPagedMode()) {
        if (!nextPageInContinuousMode())
            return false;
    } else if (m_state.nextPage()) {
        recomputeFitZoom();
        notifyChanged();
        return true;
    } else {
        return false;
    }
    return true;
}

bool ViewerController::prevPage() {
    if (!isPagedMode()) {
        if (!prevPageInContinuousMode())
            return false;
    } else if (m_state.prevPage()) {
        recomputeFitZoom();
        notifyChanged();
        return true;
    } else {
        return false;
    }
    return true;
}

bool ViewerController::firstPage() {
    if (!m_state.firstPage())
        return false;
    recomputeFitZoom();
    notifyChanged();
    return true;
}

bool ViewerController::lastPage() {
    if (!m_state.lastPage())
        return false;
    recomputeFitZoom();
    notifyChanged();
    return true;
}

bool ViewerController::goToPage(int page) {
    if (!m_state.goToPage(page))
        return false;
    recomputeFitZoom();
    notifyChanged();
    return true;
}

bool ViewerController::nextPageInContinuousMode() {
    if (m_state.nextPage()) {
        notifyChanged();
        return true;
    }
    return false;
}

bool ViewerController::prevPageInContinuousMode() {
    if (m_state.prevPage()) {
        notifyChanged();
        return true;
    }
    return false;
}

void ViewerController::zoomIn() {
    m_state.zoomIn();
    m_fitMode = FitMode::Manual;
    notifyChanged();
}

void ViewerController::zoomOut() {
    m_state.zoomOut();
    m_fitMode = FitMode::Manual;
    notifyChanged();
}

void ViewerController::setManualZoom(float zoom) {
    m_state.setZoom(zoom);
    m_fitMode = FitMode::Manual;
    notifyChanged();
}

void ViewerController::cycleFitMode() {
    switch (m_fitMode) {
    case FitMode::FitToPage:
        m_fitMode = FitMode::FitToWidth;
        break;
    case FitMode::FitToWidth:
        m_fitMode = FitMode::Manual;
        m_state.setZoom(1.0f);
        break;
    case FitMode::Manual:
        m_fitMode = FitMode::FitToPage;
        break;
    }
    recomputeFitZoom();
    notifyChanged();
}

void ViewerController::toggleMode() {
    m_state.setPagedMode(!m_state.isPagedMode());
    recomputeFitZoom();
    notifyChanged();
}

void ViewerController::rotateCw() {
    m_rotation = (m_rotation + 90) % 360;
    recomputeFitZoom();
    notifyChanged();
}

void ViewerController::rotateCcw() {
    m_rotation = (m_rotation + 270) % 360;
    recomputeFitZoom();
    notifyChanged();
}

void ViewerController::setViewportSize(const QSize& size) {
    m_viewportSize = size;
    recomputeFitZoom();
}

int ViewerController::pageAreaWidth() const {
    return std::max(1, m_viewportSize.width());
}

int ViewerController::pageAreaHeight() const {
    return std::max(1, m_viewportSize.height() - kInfoPanelHeight);
}

void ViewerController::recomputeFitZoom() {
    if (!m_engine || !m_engine->isOpen())
        return;
    if (m_fitMode == FitMode::Manual)
        return;

    PageInfo info = m_engine->pageDimensions(m_state.currentPage());
    if (info.width <= 0 || info.height <= 0)
        return;

    // Fit targets are expressed in logical (DPI-independent) pixels: the
    // rendered bitmap is page * zoom * dpiScale device pixels, so the zoom
    // that fits the physical viewport is viewport / dpiScale.
    const float invScale = 1.0f / m_dpiScale;
    const int vw = std::max(1, static_cast<int>(pageAreaWidth() * invScale));
    const int vh = std::max(1, static_cast<int>(pageAreaHeight() * invScale));

    auto rotatedInfo = info;
    if (m_rotation == 90 || m_rotation == 270) {
        std::swap(rotatedInfo.width, rotatedInfo.height);
    }

    if (m_fitMode == FitMode::FitToPage) {
        m_state.setZoom(m_state.fitToPageZoom(rotatedInfo.width, rotatedInfo.height, vw, vh));
    } else {
        m_state.setZoom(m_state.fitToWidthZoom(rotatedInfo.width, vw));
    }
}

int ViewerController::pageStride() const {
    if (!m_engine || !m_engine->isOpen())
        return 0;
    PageInfo info = m_engine->pageDimensions(m_state.currentPage());
    if (info.width <= 0 || info.height <= 0)
        return 0;
    if (m_rotation == 90 || m_rotation == 270)
        std::swap(info.width, info.height);
    int scaledPageH = static_cast<int>(info.height * m_state.zoom() * m_dpiScale);
    return scaledPageH + kPageGap;
}

int ViewerController::pageAtScrollOffset(int scrollY) const {
    if (!m_engine || !m_engine->isOpen() || m_state.pageCount() <= 0)
        return m_state.currentPage();

    int stride = pageStride();
    if (stride <= 0)
        return m_state.currentPage();

    PageInfo info = m_engine->pageDimensions(1);
    if (info.width <= 0 || info.height <= 0)
        return m_state.currentPage();
    if (m_rotation == 90 || m_rotation == 270)
        std::swap(info.width, info.height);
    int pageH = static_cast<int>(info.height * m_state.zoom() * m_dpiScale);

    int page = (scrollY + pageH / 2) / stride + 1;
    page = (std::clamp)(page, 1, m_state.pageCount());

    // When the last page is at least partially inside the viewport, report
    // it as the current page. The Win32 scrollbar cap (nMax - nPage + 1)
    // can prevent (scrollY + pageH/2) from ever landing on the final page,
    // so without this fallback the info panel is stuck on (pageCount - 1).
    const int vh = pageAreaHeight();
    if (vh > 0) {
        const int lastPageTop = (m_state.pageCount() - 1) * stride;
        if (scrollY + vh > lastPageTop)
            page = m_state.pageCount();
    }
    return page;
}

void ViewerController::trackCurrentPage(int page) {
    m_state.goToPage(page);
}

QImage ViewerController::renderVisiblePages(int scrollY) {
    if (!m_engine || !m_engine->isOpen())
        return {};

    const int vw = pageAreaWidth();
    const int vh = pageAreaHeight();
    if (vw <= 0 || vh <= 0)
        return {};

    if (isPagedMode()) {
        return m_engine->renderPage(m_state.currentPage(), m_state.zoom(), m_dpiScale, m_rotation);
    }

    const int pageCount = m_state.pageCount();
    if (pageCount <= 0)
        return {};

    PageInfo cur = m_engine->pageDimensions(m_state.currentPage());
    if (cur.width <= 0 || cur.height <= 0)
        return {};
    if (m_rotation == 90 || m_rotation == 270)
        std::swap(cur.width, cur.height);
    const int scaledPageH = static_cast<int>(cur.height * m_state.zoom() * m_dpiScale);
    const int scaledPageW = static_cast<int>(cur.width * m_state.zoom() * m_dpiScale);
    if (scaledPageH <= 0 || scaledPageW <= 0)
        return {};

    const int stride = scaledPageH + kPageGap;
    const int stripW = std::max(scaledPageW, vw);
    const int xPad = (stripW - scaledPageW) / 2;
    const long long totalH =
        static_cast<long long>(pageCount) * scaledPageH + static_cast<long long>(pageCount - 1) * kPageGap;
    const int stripH = static_cast<int>(std::min<long long>(totalH, viewer_settings::kMaxStripHeight));

    QImage strip(stripW, stripH, QImage::Format_RGB888);
    strip.fill(0x808080);

    const int visTop = scrollY;
    const int visBot = visTop + vh;
    const int firstPage = (std::max)(1, visTop / stride - viewer_settings::kBufferPages);
    const int lastPage = (std::min)(pageCount, (visBot + stride - 1) / stride + viewer_settings::kBufferPages);

    QPainter p(&strip);
    for (int page = firstPage; page <= lastPage; ++page) {
        int y = static_cast<long long>(page - 1) * stride;
        if (y >= stripH)
            break;
        QImage img = m_engine->renderPage(page, m_state.zoom(), m_dpiScale, m_rotation);
        if (!img.isNull())
            p.drawImage(xPad, y, img);
    }
    p.end();
    return strip;
}

void ViewerController::notifyChanged() {
    if (m_onChanged)
        m_onChanged();
}
