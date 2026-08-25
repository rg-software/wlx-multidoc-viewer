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
#include <limits>

#include <QDebug>
#include <QPainter>
#include <QTransform>

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
    // Any in-flight search must finish (or be cancelled and joined) before the
    // engine is torn down: the worker may be mid-searchText on the engine.
    stopSearchThread();
    ++m_searchGeneration;
    m_searchHits.clear();
    m_searchQuery.clear();
    m_activeHit = -1;
    m_searchStarted = false;
    m_searchFinished = false;
    m_searchCancelled = false;
    m_searchNoMatch = false;

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
    return std::max(1, m_viewportSize.width() - m_leftChromePx);
}

int ViewerController::pageAreaHeight() const {
    return std::max(1, m_viewportSize.height() - m_topChromePx - m_bottomChromePx);
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

    // Fit targets live in layout units (the same units as the viewport):
    // the rendered bitmap is page * zoom * renderScale pixels, but geometry
    // only ever uses page * zoom * layoutScale, so compensating by anything
    // other than layoutScale would misfit. Qt keeps both at 1; Win32 uses
    // DPI/96 for both.
    const float invScale = 1.0f / m_layoutScale;
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
    const float z = m_state.zoom() * m_layoutScale;

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
    // The "current page" in continuous mode is the page whose top is at/near
    // the top of the viewport. This advances only when you have scrolled the
    // previous page fully off (a full page height), so it never "jumps to the
    // next page" at a half-page split.
    return firstPageAtScroll(scrollY);
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

int ViewerController::maxScrollOffsetXForPage(int page) const {
    QRect r = pageRect(page);
    if (!r.isValid())
        return 0;
    return std::max(0, r.width() - pageAreaWidth());
}

int ViewerController::maxScrollOffsetYForPage(int page) const {
    QRect r = pageRect(page);
    if (!r.isValid())
        return 0;
    return std::max(0, r.height() - pageAreaHeight());
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

    QImage img = m_engine->renderPage(page, m_state.zoom(), m_renderScale, m_rotation);
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
    slice.fill(0xE8E8E8); // light page background

    const int visTop = scrollY;
    const int visBot = scrollY + vh;
    QPainter pm;
    pm.begin(&slice);
    pm.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (int p = 1; p <= m_pageRects.size(); ++p) {
        const QRect r = m_pageRects[p - 1];
        if (r.top() > visBot)
            break;
        if (r.bottom() < visTop)
            continue;
        const QImage img = renderPageCached(p);
        if (img.isNull())
            continue;
        // Bitmap is zoom*renderScale px; the layout rect is zoom*layoutScale
        // units. drawImage(target) maps one onto the other for any scale pair.
        pm.drawImage(QRect(r.x() - scrollX, r.y() - scrollY, r.width(), r.height()), img);
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

// ---------------------------------------------------------------------------
// Text selection
// ---------------------------------------------------------------------------

bool ViewerController::pageHasText(int page) const {
    if (!m_engine || !m_engine->isOpen() || page < 1 || page > m_state.pageCount())
        return false;
    return !pageText(page).words.isEmpty();
}

PageText ViewerController::pageText(int page) const {
    if (page < 1 || page > m_state.pageCount())
        return {};
    auto it = m_textCache.find(page);
    if (it != m_textCache.end())
        return it.value();
    PageText pt = m_engine ? m_engine->pageText(page) : PageText();
    m_textCache.insert(page, pt);
    return pt;
}

// Map a page-space point (y-down, page dimensions before zoom) to the content
// canvas. Word bboxes are in page space; the canvas places the page rect at
// m_pageRects[page-1].topLeft() scaled by zoom*layoutScale and rotated about the
// page center. The rotation convention matches renderPage's ctm (about center).
QTransform ViewerController::pageTransform(int page) const {
    if (page < 1 || page > m_pageRects.size())
        return {};
    const QRect r = m_pageRects[page - 1];
    const PageInfo info = m_engine ? m_engine->pageDimensions(page) : PageInfo();
    if (info.width <= 0 || info.height <= 0 || r.isEmpty())
        return {};

    const float scale = m_state.zoom() * m_layoutScale;
    const float centerX = info.width * 0.5f;
    const float centerY = info.height * 0.5f;

    // page -> (page*zoom) -> rotate about center -> +pageRect.rotation.
    QTransform t;
    t.translate(r.x() + scale * centerX,
                r.y() + scale * centerY);
    t.rotate(-m_rotation); // Qt y-down; positive rotate is clockwise, renderPage rotates CW for +90
    t.translate(-scale * centerX, -scale * centerY);
    t.scale(scale, scale);
    return t;
}

QPointF ViewerController::canvasToPagePoint(int page, const QPointF& canvasPt) const {
    return pageTransform(page).inverted().map(canvasPt);
}

// Nearest word measured in CANVAS pixels so the tolerance compares directly
// with the pointer position. tolerancePx >= 0 returns -1 when nothing is within
// that distance (empty area -> pan, not selection).
int ViewerController::wordAtCanvas(int page, const QPointF& canvasPt, double tolerancePx) const {
    const PageText pt = const_cast<ViewerController*>(this)->pageText(page);
    if (pt.words.isEmpty())
        return -1;
    const QTransform t = pageTransform(page);
    if (!t.isInvertible())
        return -1;

    double best = std::numeric_limits<double>::max();
    int bestIdx = -1;
    for (int i = 0; i < pt.words.size(); ++i) {
        const QRectF cr = t.mapRect(pt.words[i].bbox);
        const double dx = std::max({cr.left() - canvasPt.x(), 0.0, canvasPt.x() - cr.right()});
        const double dy = std::max({cr.top() - canvasPt.y(), 0.0, canvasPt.y() - cr.bottom()});
        const double d = std::sqrt(dx * dx + dy * dy);
        if (d < best) {
            best = d;
            bestIdx = i;
        }
    }
    if (tolerancePx >= 0.0 && best > tolerancePx)
        return -1;
    return bestIdx;
}

// Map an x-position within a word to a character offset (0..wordLen) by
// proportional advance across the word's bbox.
int ViewerController::charAtCanvas(int page, int wordIndex, const QPointF& canvasPt) const {
    const PageText pt = pageText(page);
    if (wordIndex < 0 || wordIndex >= pt.words.size())
        return 0;
    const TextWord& w = pt.words[wordIndex];
    const int len = w.text.size();
    if (len <= 0)
        return 0;
    const QTransform t = pageTransform(page);
    if (!t.isInvertible())
        return 0;
    const QPointF p = t.inverted().map(canvasPt);
    const QRectF b = w.bbox;
    if (b.width() <= 0.0)
        return 0;
    const double fx = (p.x() - b.left()) / b.width();
    return (std::clamp)(static_cast<int>(std::round(fx * len)), 0, len);
}

QRectF ViewerController::wordRectOnCanvas(int page, int wordIndex) const {
    const PageText pt = pageText(page);
    if (wordIndex < 0 || wordIndex >= pt.words.size())
        return {};
    return pageTransform(page).mapRect(pt.words[wordIndex].bbox);
}

void ViewerController::beginSelection(int page, int wordIndex, int charIndex) {
    // Only start a selection over selectable text.
    if (!pageHasText(page) || wordIndex < 0)
        return;
    m_textSelection.begin(page, wordIndex, charIndex);
    m_selecting = true;
    notifyChanged();
}

void ViewerController::updateSelection(int page, int wordIndex, int charIndex) {
    if (!m_selecting || wordIndex < 0)
        return;
    m_textSelection.setFocus(page, wordIndex, charIndex);
    notifyChanged();
}

void ViewerController::endSelection() {
    m_selecting = false;
    // Selection stays active after release (highlights persist).
}

void ViewerController::clearSelection() {
    if (!m_textSelection.isActive() && !m_selecting)
        return; // avoid spurious repaints
    m_selecting = false;
    m_textSelection.clear();
    notifyChanged();
}

QVector<QRectF> ViewerController::highlightRects(int page) const {
    QVector<QRectF> out;
    if (!m_textSelection.isActive())
        return out;
    const PageText pt = pageText(page);
    if (pt.words.isEmpty())
        return out;
    const QTransform t = pageTransform(page);
    if (!t.isInvertible())
        return out;
    const QVector<SelectionCharSpan> spans =
        m_textSelection.charSpansOnPage(page, pt.words.size());
    if (spans.isEmpty())
        return out;

    // Per-word slice rects; union contiguous spans that share a line into one
    // highlight box (partial first/last words keep their cut boundaries).
    QRectF run;
    int runLine = -1;
    bool inRun = false;
    for (const SelectionCharSpan& sp : spans) {
        if (sp.wordIndex < 0 || sp.wordIndex >= pt.words.size())
            continue;
        const TextWord& w = pt.words[sp.wordIndex];
        const int len = w.text.size();
        QRectF slice = w.bbox;
        if (len > 0 && w.bbox.width() > 0.0) {
            const int from = (std::clamp)(sp.from, 0, len);
            const int to = (sp.to < 0) ? len : (std::clamp)(sp.to, 0, len);
            if (to > from) {
                slice.setLeft(w.bbox.left() + w.bbox.width() * (from / static_cast<double>(len)));
                slice.setRight(w.bbox.left() + w.bbox.width() * (to / static_cast<double>(len)));
            }
        }
        if (w.lineIndex != runLine) {
            if (inRun)
                out.append(t.mapRect(run.normalized()));
            runLine = w.lineIndex;
            run = slice;
            inRun = true;
        } else {
            run = run.united(slice);
        }
    }
    if (inRun)
        out.append(t.mapRect(run.normalized()));
    return out;
}

QString ViewerController::selectedText() const {
    if (!m_textSelection.isActive())
        return {};
    QString result;
    int prevPage = -1;
    int prevLine = -1;
    for (int page = m_textSelection.firstPage(); page <= m_textSelection.lastPage(); ++page) {
        const PageText pt = pageText(page);
        if (pt.words.isEmpty())
            continue;
        const QVector<SelectionCharSpan> spans =
            m_textSelection.charSpansOnPage(page, pt.words.size());
        for (const SelectionCharSpan& sp : spans) {
            if (sp.wordIndex < 0 || sp.wordIndex >= pt.words.size())
                continue;
            const TextWord& w = pt.words[sp.wordIndex];
            const int len = w.text.size();
            if (len <= 0)
                continue;
            const int from = (std::clamp)(sp.from, 0, len);
            const int to = (sp.to < 0) ? len : (std::clamp)(sp.to, 0, len);
            if (to <= from)
                continue;
            const QString piece = w.text.mid(from, to - from);
            if (page != prevPage || w.lineIndex != prevLine) {
                if (!result.isEmpty())
                    result += '\n';
                prevPage = page;
                prevLine = w.lineIndex;
            } else {
                result += ' ';
            }
            result += piece;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Text search
// ---------------------------------------------------------------------------

bool ViewerController::startSearch(const QString& term, bool matchCase) {
    if (!searchAvailable() || term.isEmpty()) {
        clearSearch();
        return false;
    }

    // A new search supersedes any text selection so only match highlights show.
    if (m_textSelection.isActive()) {
        m_textSelection.clear();
        m_selecting = false;
    }

    // Restart semantics: cancel/join any running scan, then reset state.
    stopSearchThread();
    const int generation = ++m_searchGeneration;
    m_searchHits.clear();
    m_activeHit = -1;
    m_searchQuery = term;
    m_matchCase = matchCase;
    m_searchStarted = false;
    m_searchFinished = false;
    m_searchCancelled = false;
    m_searchNoMatch = false;
    m_searchJumpPending = false;

    if (!m_search)
        m_search = std::make_unique<SearchController>();

    // Copy the marshaller so the worker lambdas do not touch shared state.
    const UiMarshalFn marshal = m_uiMarshal;
    const bool started = m_search->start(
        m_engine.get(), m_state.pageCount(), m_state.currentPage(), term, matchCase,
        [this, marshal, generation](const QVector<TextMatch>& matches) {
            std::function<void()> task = [this, matches, generation]() {
                receiveSearchPage(matches, generation);
            };
            if (marshal) marshal(std::move(task)); else task();
        },
        [this, marshal, generation](bool cancelled) {
            std::function<void()> task = [this, cancelled, generation]() {
                receiveSearchComplete(cancelled, generation);
            };
            if (marshal) marshal(std::move(task)); else task();
        });

    m_searchStarted = started;
    notifyChanged();
    return started;
}

void ViewerController::clearSearch() {
    if (m_searchQuery.isEmpty() && m_searchHits.isEmpty() && !m_searchStarted)
        return;
    stopSearchThread();
    ++m_searchGeneration;
    m_searchHits.clear();
    m_activeHit = -1;
    m_searchQuery.clear();
    m_searchStarted = false;
    m_searchFinished = false;
    m_searchCancelled = false;
    m_searchNoMatch = false;
    m_searchJumpPending = false;
    notifyChanged();
}

void ViewerController::stopSearchThread() {
    if (m_search) {
        m_search->cancel();
        m_search->join();
    }
}

void ViewerController::receiveSearchPage(const QVector<TextMatch>& matches, int generation) {
    if (generation != m_searchGeneration || m_searchQuery.isEmpty())
        return;
    if (matches.isEmpty())
        return;
    const bool hadHits = !m_searchHits.isEmpty();
    for (const TextMatch& m : matches) {
        for (const QRectF& r : m.rects)
            m_searchHits.append(SearchHit{m.page, r});
    }
    // The worker scans forward from the start page, so the first hit appended
    // is the first match at or after the starting reading position.
    if (!hadHits)
        m_activeHit = 0;
    notifyChanged();
}

void ViewerController::receiveSearchComplete(bool cancelled, int generation) {
    if (generation != m_searchGeneration)
        return;
    m_searchFinished = true;
    m_searchCancelled = cancelled;
    if (!cancelled) {
        if (m_searchHits.isEmpty()) {
            m_searchNoMatch = true;
        } else {
            if (m_activeHit < 0)
                m_activeHit = 0;
            // Signal the viewer to bring the first match into view.
            m_searchJumpPending = true;
        }
    }
    notifyChanged();
}

int ViewerController::takeSearchJump() {
    if (!m_searchJumpPending) {
        // Resolve is cheap; still fall through so callers can clear state.
    }
    m_searchJumpPending = false;
    if (m_activeHit < 0 || m_activeHit >= m_searchHits.size())
        return 0;
    m_state.goToPage(m_searchHits[m_activeHit].page);
    return scrollToActiveMatch(0);
}

bool ViewerController::searchActive() const {
    return !m_searchHits.isEmpty() || m_searchStarted || searchNoMatch();
}

QRectF ViewerController::normalizedHitToCanvas(const SearchHit& hit) const {
    const PageInfo info = m_engine ? m_engine->pageDimensions(hit.page) : PageInfo();
    if (info.width <= 0 || info.height <= 0)
        return {};
    const QRectF pageSpace(hit.normalized.x() * info.width,
                           hit.normalized.y() * info.height,
                           hit.normalized.width() * info.width,
                           hit.normalized.height() * info.height);
    const QTransform t = pageTransform(hit.page);
    if (!t.isInvertible())
        return {};
    return t.mapRect(pageSpace);
}

QVector<QRectF> ViewerController::searchRectsOnPage(int page) const {
    QVector<QRectF> out;
    if (m_searchHits.isEmpty() || !hasDocument())
        return out;
    for (int i = 0; i < m_searchHits.size(); ++i) {
        if (m_searchHits[i].page != page)
            continue;
        if (i == m_activeHit)
            continue; // drawn separately with the distinct active style
        const QRectF r = normalizedHitToCanvas(m_searchHits[i]);
        if (r.isValid())
            out.append(r);
    }
    return out;
}

QRectF ViewerController::activeSearchRectOnPage(int page) const {
    if (m_activeHit < 0 || m_activeHit >= m_searchHits.size() || !hasDocument())
        return {};
    const SearchHit& hit = m_searchHits[m_activeHit];
    if (hit.page != page)
        return {};
    return normalizedHitToCanvas(hit);
}

int ViewerController::nextMatch(int scrollY) {
    if (m_searchHits.isEmpty())
        return scrollY;
    if (m_activeHit < 0)
        m_activeHit = 0;
    else
        m_activeHit = (m_activeHit + 1) % m_searchHits.size();
    m_state.goToPage(m_searchHits[m_activeHit].page);
    const int out = scrollToActiveMatch(scrollY);
    notifyChanged();
    return out;
}

int ViewerController::prevMatch(int scrollY) {
    if (m_searchHits.isEmpty())
        return scrollY;
    if (m_activeHit < 0)
        m_activeHit = 0;
    else
        m_activeHit = (m_activeHit > 0) ? m_activeHit - 1 : m_searchHits.size() - 1;
    m_state.goToPage(m_searchHits[m_activeHit].page);
    const int out = scrollToActiveMatch(scrollY);
    notifyChanged();
    return out;
}

// Computes the scroll offset that brings the active match into view (upper
// third of the viewport). Paged mode returns an in-page overflow offset when
// the page is taller than the viewport; the surrounding commands already moved
// the current page to the match's page.
int ViewerController::scrollToActiveMatch(int scrollY) const {
    Q_UNUSED(scrollY)
    if (m_activeHit < 0 || m_activeHit >= m_searchHits.size())
        return 0;
    const SearchHit& hit = m_searchHits[m_activeHit];
    const QRectF canvas = normalizedHitToCanvas(hit);
    if (canvas.isNull())
        return 0;
    const int vh = pageAreaHeight();
    if (m_state.isPagedMode()) {
        const QRect pr = pageRect(hit.page);
        const int rel = std::max(0, static_cast<int>(canvas.center().y()) - pr.y());
        const int maxRel = std::max(0, pr.height() - vh);
        return (std::clamp)(rel - vh / 3, 0, maxRel);
    }
    return clampScroll(std::max(0, static_cast<int>(canvas.center().y()) - vh / 3));
}