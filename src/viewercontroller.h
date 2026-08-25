#ifndef VIEWERCONTROLLER_H
#define VIEWERCONTROLLER_H

#include "document.h"
#include "searchcontroller.h"
#include "textselection.h"
#include "viewer_settings.h"
#include "viewerstate.h"

#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QString>
#include <QTransform>
#include <QVector>
#include <algorithm>
#include <functional>
#include <memory>

class ViewerController {
public:
    enum class FitMode { Manual, FitToPage, FitToWidth };

    using StateChangedCallback = std::function<void()>;

    ViewerController();

    void setEngine(std::unique_ptr<DocumentEngine> engine);
    DocumentEngine* engine() const { return m_engine.get(); }

    bool openDocument(const QString& path);
    void closeDocument();

    // Navigation
    bool nextPage();
    bool prevPage();
    bool firstPage();
    bool lastPage();
    bool goToPage(int page);
    bool nextPageInContinuousMode();
    bool prevPageInContinuousMode();

    // Zoom / fit. Each layout-affecting command takes the current continuous
    // scroll offset and returns the new offset that keeps the view anchored to
    // the same document region. In paged mode the offset is unchanged (0).
    int zoomIn(int scrollY);
    int zoomOut(int scrollY);
    int setManualZoom(float zoom, int scrollY);
    int cycleFitMode(int scrollY);
    int rotateCw(int scrollY);
    int rotateCcw(int scrollY);

    // Display mode
    void toggleMode();
    bool isPagedMode() const { return m_state.isPagedMode(); }

    // Viewport
    void setViewportSize(const QSize& size);
    int pageAreaHeight() const;
    int pageAreaWidth() const;
    // Two scale roles, both defaulting to 1:
    // - layoutScale multiplies page geometry (pageRects, contentSize). Win32
    //   uses DPI/96 because HWND coordinates are physical device pixels; Qt
    //   keeps 1.0 because widget coordinates are logical (HiDPI is applied by
    //   the backing store, not by our geometry).
    // - renderScale is bitmap density only: pages are rasterized at
    //   zoom * renderScale pixels so one bitmap pixel maps onto one physical
    //   screen pixel at any display scaling.
    void setLayoutScale(float scale) { m_layoutScale = clampScale(scale); }
    void setRenderScale(float scale) { m_renderScale = clampScale(scale); }
    float layoutScale() const { return m_layoutScale; }
    float renderScale() const { return m_renderScale; }
    // Chrome (toolbar strip top, optional sidebar left)
    // subtracted from the viewport size to form the page area. Device pixels,
    // set by the viewers.
    void setTopChrome(int px) { m_topChromePx = std::max(0, px); }
    void setBottomChrome(int px) { m_bottomChromePx = std::max(0, px); }
    void setLeftChrome(int px) { m_leftChromePx = std::max(0, px); }
    // Scroll anchor the UI thread keeps fresh so toolbar actions (zoom, fit,
    // rotate, match navigation) can anchor the view like the keyboard paths.
    void setScrollAnchor(int y) { m_scrollAnchor = std::max(0, y); }
    int scrollAnchor() const { return m_scrollAnchor; }

    // Render
    QImage renderPageCached(int page);
    QImage renderCachedViewport(int scrollY = 0, int scrollX = 0);

    // Continuous-mode virtual canvas.
    QSize contentSize() const { return m_contentSize; }
    QRect pageRect(int page) const;
    int pageAtScrollOffset(int scrollY) const;
    int firstPageAtScroll(int scrollY) const;
    int maxScrollOffset() const;
    int maxScrollOffsetX() const;
    int maxScrollOffsetXForPage(int page) const;
    int maxScrollOffsetYForPage(int page) const;
    int scrollOffsetForPage(int page) const;
    void trackCurrentPage(int page);
    void trimRenderCache(int scrollY);
    int layoutEpoch() const { return m_layoutEpoch; }

    // ---- Text selection ----
    bool pageHasText(int page) const;
    PageText pageText(int page) const;
    QTransform pageTransform(int page) const;
    QPointF canvasToPagePoint(int page, const QPointF& canvasPt) const;
    // Nearest word under canvasPt. tolerancePx >= 0 restricts the hit to that
    // distance in canvas pixels (returns -1 when the point is in empty space),
    // so empty page areas pan instead of starting a text selection. -1 = always
    // nearest, used while extending an active selection.
    int wordAtCanvas(int page, const QPointF& canvasPt, double tolerancePx = -1.0) const;
    // Character offset (0..wordLen) at an x-position within a word.
    int charAtCanvas(int page, int wordIndex, const QPointF& canvasPt) const;
    QRectF wordRectOnCanvas(int page, int wordIndex) const;
    void beginSelection(int page, int wordIndex, int charIndex = 0);
    void updateSelection(int page, int wordIndex, int charIndex = 0);
    void endSelection();
    void clearSelection();
    bool hasSelection() const { return m_textSelection.isActive(); }
    QVector<QRectF> highlightRects(int page) const;
    QString selectedText() const;

    // Relays a fit/zoom mode change that the caller already applied (fit mode,
    // viewport size, rotation angle) into a fresh layout, anchoring scrollY.
    int relayout(int scrollY);

    // ---- Text search (whole document) ----
    // Search results are delivered asynchronously. The platform viewer installs
    // a marshaller that runs a task on its UI thread (PostMessage / queued
    // invocation); without one the callbacks run inline on the worker thread.
    using UiMarshalFn = std::function<void(std::function<void()>)>;

    bool searchAvailable() const { return m_engine && m_engine->supportsSearch(); }
    bool startSearch(const QString& term, bool matchCase);
    void clearSearch();
    bool searchActive() const;                 // results, a running scan, or no-match state
    bool searchInProgress() const { return m_searchStarted && !m_searchFinished; }
    bool searchNoMatch() const { return m_searchFinished && !m_searchCancelled && m_searchNoMatch; }
    int searchMatchCount() const { return m_searchHits.size(); }
    int activeMatchIndex() const { return m_activeHit; }
    QString searchQuery() const { return m_searchQuery; }
    // Iteration: moves the active match by delta (-1/+1, wrapping at
    // boundaries) and brings it into view. Returns the new scroll offset for
    // the caller to apply.
    int nextMatch(int scrollY);
    int prevMatch(int scrollY);
    // Canvas-space highlight rects for the overlay (all matches, or the active
    // match for the distinct style).
    bool hasSearchHighlights() const { return !m_searchHits.isEmpty(); }
    QVector<QRectF> searchRectsOnPage(int page) const;
    QRectF activeSearchRectOnPage(int page) const;

    void setUiMarshal(UiMarshalFn fn) { m_uiMarshal = std::move(fn); }

    // Accessors
    int currentPage() const { return m_state.currentPage(); }
    int pageCount() const { return m_state.pageCount(); }
    float zoom() const { return m_state.zoom(); }
    FitMode fitMode() const { return m_fitMode; }
    int rotation() const { return m_rotation; }
    bool hasDocument() const { return m_engine && m_engine->isOpen(); }

    void setStateChangedCallback(StateChangedCallback cb) { m_onChanged = std::move(cb); }

private:
    struct SearchHit {
        int page = 0;
        QRectF normalized; // 0..1 page space
    };

    // Whole-document search state. m_searchHits is the flattened, ordered
    // match list (the worker scans forward from the current page, so index 0 is
    // the first match at or after the starting position). The generation guard
    // lets stale marshaled callbacks from a superseded scan be ignored.
    std::unique_ptr<SearchController> m_search;
    QVector<SearchHit> m_searchHits;
    QString m_searchQuery;
    bool m_matchCase = false;
    int m_activeHit = -1;
    bool m_searchStarted = false;
    bool m_searchFinished = false;
    bool m_searchCancelled = false;
    bool m_searchNoMatch = false;
    int m_searchGeneration = 0;
    mutable bool m_searchJumpPending = false;
    UiMarshalFn m_uiMarshal;

    void computeFitZoom();
    void computeLayout();
    int clampScroll(int scrollY) const;
    void notifyChanged();

    // Search helpers (all UI-thread entries).
    void stopSearchThread();
    void receiveSearchPage(const QVector<TextMatch>& matches, int generation);
    void receiveSearchComplete(bool cancelled, int generation);
    QRectF normalizedHitToCanvas(const SearchHit& hit) const;
    int scrollToActiveMatch(int scrollY) const;

public:
    // True right after a search that found matches; the viewer should bring the
    // first (current-position-nearest) match into view. One-shot flag.
    bool hasPendingSearchJump() const { return m_searchJumpPending; }
    // Resolves the jump and clears the flag. Returns the scroll offset to apply
    // (clamped); paged mode already navigated to the match's page.
    int takeSearchJump();

private:
    static float clampScale(float s) { return std::max(0.25f, std::min(s, 8.0f)); }

    std::unique_ptr<DocumentEngine> m_engine;
    ViewerState m_state;
    FitMode m_fitMode = FitMode::FitToPage;
    int m_rotation = 0;
    QSize m_viewportSize;
    float m_layoutScale = 1.0f;
    float m_renderScale = 1.0f;
    StateChangedCallback m_onChanged;
    int m_topChromePx = 0;
    int m_bottomChromePx = 0;
    int m_leftChromePx = 0;
    int m_scrollAnchor = 0;

    // Per-page layout in canvas units (logical px on Qt, device px on Win32).
    // m_pageRects[page-1].top() is the
    // cumulative top of the page in the (unbounded) content canvas; the canvas
    // width is max(page widths, viewport width) and pages are centered in it.
    QVector<QRect> m_pageRects;
    QSize m_contentSize;
    int m_layoutEpoch = 0;

    // Per-page render cache. m_pageCache[page-1] is null when not rendered;
    // m_cacheRecency is most-recent-first for eviction beyond the cache window.
    QVector<QImage> m_pageCache;
    QVector<int> m_cacheRecency;

    // Text-selection state.
    mutable QHash<int, PageText> m_textCache;
    TextSelection m_textSelection;
    bool m_selecting = false;
};

#endif // VIEWERCONTROLLER_H