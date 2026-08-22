// Design notes (SumatraPDF reference patterns; read-only, not copied)
// ---------------------------------------------------
// Continuous mode uses a *virtual canvas* + per-page layout instead of one
// tall strip bitmap (see AGENTS.md for the gaps this closes):
//
//   - SumatraPDF's DisplayModel keeps per-page PageInfo.pos rects laid out in a
//     canvas whose height is the sum of page heights + gaps, and drives the
//     scrollbar from that canvas (nMax = canvas.dy - 1). We expose the same
//     via contentSize()/pageRect()/maxScrollOffset().
//
//   - Rendering is per-page on demand: Sumatra paints from a bitmaps cache and
//     shows a grey placeholder for not-yet-rendered pages. Our page render
//     cache is the synchronous form; a background worker (async-render-worker
//     change) will make it non-blocking.
//
//   - Relayout keeps the view anchored: Sumatra's RelayoutKeepingView() restores
//     viewPort.y = firstVisiblePage.pos.y + dyInPage. Our relayout() and the
//     anchored zoom/fit/rotate commands do the same fraction-based math.
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
using viewer_settings::kCacheWindowPages;
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
    m_pageCache.clear();
    m_cacheRecency.clear();
    computeFitZoom();
    computeLayout();
    notifyChanged();
    return true;
}

void ViewerController::closeDocument() {
    if (m_engine)
        m_engine->close();
    m_state = ViewerState();
    m_fitMode = FitMode::FitToPage;
    m_rotation = 0;
    m_pageRects.clear();
    m_contentSize = QSize();
    m_pageCache.clear();
    m_cacheRecency.clear();
    notifyChanged();
}

bool ViewerController::nextPage() {
    if (!isPagedMode())
        return nextPageInContinuousMode();
    if (m_state.nextPage()) {
        notifyChanged();
        return true;
    }
    return false;
}

bool ViewerController::prevPage() {
    if (!isPagedMode())
        return prevPageInContinuousMode();
    if (m_state.prevPage()) {
        notifyChanged();
        return true;
    }
    return false;
}

bool ViewerController::firstPage() {
    if (!m_state.firstPage())
        return false;
    notifyChanged();
    return true;
}

bool ViewerController::lastPage() {
    if (!m_state.lastPage())
        return false;
    notifyChanged();
    return true;
}

bool ViewerController::goToPage(int page) {
    if (!m_state.goToPage(page))
        return false;
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

// ---------------------------------------------------------------------------
// Anchored zoom / fit / rotation. Each captures the "first-visible page +
// relative offset" from the *current* layout, applies the change, rebuilds the
// layout, then restores the viewport onto the same document region. Returns the
// new scroll offset for the caller to apply (always 0 in paged mode).
// ---------------------------------------------------------------------------

int ViewerController::zoomIn(int scrollY) {
    m_state.zoomIn();
    m_fitMode = FitMode::Manual;
    return relayout(scrollY);
}

int ViewerController::zoomOut(int scrollY) {
    m_state.zoomOut();
    m_fitMode = FitMode::Manual;
    return relayout(scrollY);
}

int ViewerController::setManualZoom(float zoom, int scrollY) {
    m_state.setZoom(zoom);
    m_fitMode = FitMode::Manual;
    return relayout(scrollY);
}

int ViewerController::cycleFitMode(int scrollY) {
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
    return relayout(scrollY);
}

int ViewerController::rotateCw(int scrollY) {
    m_rotation = (m_rotation + 90) % 360;
    return relayout(scrollY);
}

int ViewerController::rotateCcw(int scrollY) {
    m_rotation = (m_rotation + 270) % 360;
    return relayout(scrollY);
}

void ViewerController::toggleMode() {
    m_state.setPagedMode(!m_state.isPagedMode());
    computeLayout();
    notifyChanged();
}

void ViewerController::setViewportSize(const QSize& size) {
    m_viewportSize = size;
    // Layout is not recomputed here: callers follow up with relayout(scrollY)
    // so the viewport change keeps the scroll anchor.
}

int ViewerController::pageAreaWidth() const {
    return std::max(1, m_viewportSize.width());
}

int ViewerController::pageAreaHeight() const {
    return std::max(1, m_viewportSize.height() - kInfoPanelHeight);
}

// Re-layout using the current zoom/rotation/fit, anchoring scrollY to the same
// document region that was at the top of the viewport. Call AFTER the layout
// inputs (zoom, rotation, fit mode) have been mutated but BEFORE any rebuild.
// In paged mode the anchor is the current page (scroll resets to 0).
int ViewerController::relayout(int scrollY) {
    if (!hasDocument()) {
        computeFitZoom();
        computeLayout();
        return 0;
    }

    int anchorPage = m_state.currentPage();
    float frac = 0.0f;
    if (!isPagedMode() && !m_pageRects.isEmpty()) {
        anchorPage = firstPageAtScroll(scrollY);
        const QRect oldRect = m_pageRects[anchorPage - 1];
        if (oldRect.height() > 0)
            frac = (std::clamp)(
                static_cast<float>(scrollY - oldRect.y()) / static_cast<float>(oldRect.height()),
                0.0f, 1.0f);
    }

    computeFitZoom();
    computeLayout();

    int newScroll = 0;
    if (!isPagedMode() && anchorPage >= 1 && anchorPage <= m_pageRects.size()) {
        const QRect newRect = m_pageRects[anchorPage - 1];
        newScroll = newRect.y() + static_cast<int>(frac * newRect.height());
        newScroll = clampScroll(newScroll);
    }
    notifyChanged();
    return newScroll;
}

void ViewerController::computeFitZoom() {
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

// Build m_pageRects + m_contentSize from the current zoom/rotation/dpi. Each
// page uses its own scaled dimensions; the canvas width is the widest page (or
// the viewport when it is wider) and every page is horizontally centered.
void ViewerController::computeLayout() {
    m_pageRects.clear();
    m_contentSize = QSize();
    // Any layout change invalidates the rendered-page cache: cached pages were
    // produced under the old zoom/rotation/dpi.
    m_pageCache.fill(QImage(), m_state.pageCount());
    m_cacheRecency.clear();
    ++m_layoutEpoch;
    if (!m_engine || !m_engine->isOpen() || m_state.pageCount() <= 0)
        return;

    const int vw = pageAreaWidth();
    const int vh = pageAreaHeight();
    const float z = m_state.zoom() * m_dpiScale;

    QVector<QSize> sizes;
    sizes.reserve(m_state.pageCount());
    int widest = vw;
    for (int page = 1; page <= m_state.pageCount(); ++page) {
        PageInfo info = m_engine->pageDimensions(page);
        int pw = info.width;
        int ph = info.height;
        if (pw <= 0 || ph <= 0) {
            // Viewport-sized fallback keeps the rect array dense if a page
            // measurement fails (rare; engines virtually always succeed).
            pw = std::max(1, vw);
            ph = std::max(1, vh);
        } else if (m_rotation == 90 || m_rotation == 270) {
            std::swap(pw, ph);
        }
        const int sw = std::max(1, static_cast<int>(pw * z));
        const int sh = std::max(1, static_cast<int>(ph * z));
        sizes.append(QSize(sw, sh));
        widest = std::max(widest, sw);
    }

    m_contentSize.setWidth(widest);

    int cursor = 0;
    for (int i = 0; i < sizes.size(); ++i) {
        const QSize s = sizes[i];
        m_pageRects.append(QRect((widest - s.width()) / 2, cursor, s.width(), s.height()));
        cursor += s.height();
        if (i + 1 < sizes.size())
            cursor += kPageGap;
    }
    m_contentSize.setHeight(std::max(cursor, 0));
}

int ViewerController::firstPageAtScroll(int scrollY) const {
    if (m_pageRects.isEmpty())
        return m_state.currentPage();

    // First page whose bottom() is strictly greater than scrollY (i.e. the page
    // the top of the viewport is over). Grays out inter-page gaps to the page
    // above. O(log n). No last-page fallback: painting must start from the page
    // actually at the top edge, which at the document end may be the
    // penultimate page while the viewport still shows the last page below it.
    int lo = 1;
    int hi = m_pageRects.size();
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (m_pageRects[mid - 1].bottom() <= scrollY)
            lo = mid + 1;
        else
            hi = mid;
    }
    return (std::clamp)(lo, 1, m_state.pageCount());
}

int ViewerController::pageAtScrollOffset(int scrollY) const {
    if (m_pageRects.isEmpty())
        return m_state.currentPage();

    const int visBottom = scrollY + pageAreaHeight();
    const int first = firstPageAtScroll(scrollY);

    // Most-visible page: the page that occupies the largest share of the
    // viewport. At the document end the last page only takes over once it is
    // the dominant page, so the counter advances page-by-page (…, 132, 133,
    // 134) instead of jumping to the last page the moment it peeks into the
    // viewport bottom.
    int bestPage = first;
    int bestVisible = -1;
    for (int p = first; p <= m_pageRects.size(); ++p) {
        const QRect r = m_pageRects[p - 1];
        if (r.y() > visBottom)
            break;
        const int top = (std::max)(r.y(), scrollY);
        const int bot = (std::min)(r.bottom(), visBottom);
        const int visible = bot - top;
        if (visible > bestVisible) {
            bestVisible = visible;
            bestPage = p;
        }
    }
    return (std::clamp)(bestPage, 1, m_state.pageCount());
}

QRect ViewerController::pageRect(int page) const {
    if (page < 1 || page > m_pageRects.size())
        return {};
    return m_pageRects[page - 1];
}

int ViewerController::maxScrollOffset() const {
    return std::max(0, m_contentSize.height() - pageAreaHeight());
}

int ViewerController::maxScrollOffsetX() const {
    return std::max(0, m_contentSize.width() - pageAreaWidth());
}

int ViewerController::scrollOffsetForPage(int page) const {
    QRect r = pageRect(page);
    return r.isValid() ? r.y() : 0;
}

int ViewerController::clampScroll(int scrollY) const {
    return (std::clamp)(scrollY, 0, maxScrollOffset());
}

void ViewerController::trackCurrentPage(int page) {
    m_state.goToPage(page);
}

// ---------------------------------------------------------------------------
// Render cache
// ---------------------------------------------------------------------------

QImage ViewerController::renderPageCached(int page) {
    if (!m_engine || !m_engine->isOpen() || page < 1 || page > m_state.pageCount())
        return {};

    const int idx = page - 1;
    if (idx >= m_pageCache.size())
        m_pageCache.fill(QImage(), m_state.pageCount());

    if (!m_pageCache[idx].isNull()) {
        const int pos = m_cacheRecency.indexOf(idx);
        if (pos > 0) {
            m_cacheRecency.removeAt(pos);
            m_cacheRecency.prepend(idx);
        }
        return m_pageCache[idx];
    }

    QImage img = m_engine->renderPage(page, m_state.zoom(), m_dpiScale, m_rotation);
    m_pageCache[idx] = img;
    const int pos = m_cacheRecency.indexOf(idx);
    if (pos >= 0)
        m_cacheRecency.removeAt(pos);
    m_cacheRecency.prepend(idx);
    return img;
}

// Build a viewport-height slice of the content canvas at (scrollX, scrollY).
// The Qt viewer paints this directly; it is also the shared "compose" helper.
QImage ViewerController::renderCachedViewport(int scrollY, int scrollX) {
    const int vw = pageAreaWidth();
    const int vh = pageAreaHeight();
    if (vw <= 0 || vh <= 0)
        return {};
    if (isPagedMode() || m_pageRects.isEmpty())
        return renderPageCached(m_state.currentPage());

    QImage slice(vw, vh, QImage::Format_RGB888);
    slice.fill(0x808080);

    const int visTop = scrollY;
    const int visBot = scrollY + vh;
    QPainter pm;
    pm.begin(&slice);
    for (int p = 1; p <= m_pageRects.size(); ++p) {
        const QRect r = m_pageRects[p - 1];
        if (r.top() > visBot)
            break;
        if (r.bottom() < visTop)
            continue;
        const QImage img = renderPageCached(p);
        if (img.isNull())
            continue;
        pm.drawImage(r.x() - scrollX, r.y() - scrollY, img);
    }
    pm.end();
    return slice;
}

// Evict cached pages that fall outside the viewport window plus the buffer,
// and cap the total cache size to kCacheWindowPages (LRU).
void ViewerController::trimRenderCache(int scrollY) {
    if (!hasDocument() || isPagedMode())
        return;

    const int firstVisible = firstPageAtScroll(scrollY);
    // Estimate the page range that can be visible in the viewport.
    int lastVisible = firstVisible;
    const int visBottom = scrollY + pageAreaHeight();
    for (int p = firstVisible; p <= m_pageRects.size(); ++p) {
        if (m_pageRects[p - 1].bottom() > visBottom)
            break;
        lastVisible = p;
    }

    const int lo = std::max(1, firstVisible - viewer_settings::kBufferPages);
    const int hi = std::min(static_cast<int>(m_pageRects.size()),
                            lastVisible + viewer_settings::kBufferPages);

    // Drop everything outside [lo, hi]...
    for (int i = m_cacheRecency.size() - 1; i >= 0; --i) {
        const int idx = m_cacheRecency[i];
        if (idx + 1 < lo || idx + 1 > hi) {
            m_pageCache[idx] = QImage();
            m_cacheRecency.removeAt(i);
        }
    }
    // ...and cap the window as a memory bound.
    while (m_cacheRecency.size() > viewer_settings::kCacheWindowPages) {
        const int victim = m_cacheRecency.back();
        m_cacheRecency.pop_back();
        m_pageCache[victim] = QImage();
    }
}

void ViewerController::notifyChanged() {
    if (m_onChanged)
        m_onChanged();
}