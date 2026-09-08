#ifndef VIEWERCONTROLLER_H
#define VIEWERCONTROLLER_H

#include "document.h"
#include "imagefolder.h"
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
    // Whether a next/previous SCREEN (unit) exists. In a double-page state the
    // last unit may contain the final page(s), so currentPage < pageCount alone
    // is not enough to decide if "next" can advance.
    bool hasNextPage() const;
    bool hasPrevPage() const;

    // Standalone-image folder navigation. For a raster image document these
    // report whether a sibling raster image exists on the given side so next/prev
    // can cross the single-page document boundary instead of clamping.
    bool hasNextSibling() const;
    bool hasPrevSibling() const;

    // Folder position for the page indicator of a standalone image document:
    // 1-based index within the sibling set (0 when not in a folder context) and
    // the total sibling count (1 when isolated).
    int imagePosition() const { return m_folderIndex >= 0 ? m_folderIndex + 1 : 0; }
    int imageCount() const { return m_folder.count(); }

    // ---- In-place animation (GIF) ----
    // The platform viewer arms a timer for animationDelayMs() and calls
    // animationTick() on each tick. animationTick() advances a frame (invalidating
    // the page cache so repaint re-decodes it) and returns whether playback should
    // continue; when true the caller re-arms with the new animationDelayMs().
    // animationEpoch() ticks up on every frame advance so viewers can narrow-repaint
    // the page region without a full relayout (task 5.2).
    bool isAnimating() const;
    int animationDelayMs() const;
    bool animationTick();
    int animationEpoch() const { return m_animationEpoch; }

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

    // Page presentation (single / double / double-with-cover). cyclePagePresentation
    // advances single -> double -> double-with-cover -> single and re-resolves the
    // current page to its unit under the new state.
    void cyclePagePresentation();
    ViewerState::PagePresentation pagePresentation() const { return m_state.pagePresentation(); }

    // Unit pairing: a unit is the set of pages displayed together (one in single,
    // two in a double-page state; cover mode isolates page 1). unitFirst returns the
    // first (leftmost) page of the unit containing page; unitLast returns the unit's
    // second member, or the page itself when the unit is a singleton.
    int unitFirst(int page) const;
    int unitLast(int unitFirst) const;
    bool isDoublePagePresentation() const {
        return m_state.pagePresentation() != ViewerState::PagePresentation::Single;
    }
    // Continuous-mode screen step: the target page when the view is advanced by
    // one whole screen (+1) or moved back (-1) from the page currently at the
    // top of the viewport. In a double-page presentation the two members of a
    // unit move together, so the step jumps to the neighbouring unit (and is a
    // no-op past the first/last unit) instead of to the unit's other member.
    int scrollStepPage(int base, int delta) const;

    // Viewport
    void setViewportSize(const QSize& size);
    int pageAreaHeight() const;
    int pageAreaWidth() const;
    // PgUp/PgDn advance by one screenful of page area, keeping a small overlap
    // band at the edge so the previous screen's last line stays readable.
    int pageBlockStep() const;
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
    // Overflow limits for the whole unit containing unitFirstPage (the page
    // pair in a double-page state), not just the single page. Paged viewers use
    // these so a wide/tall spread can be panned as one group.
    int maxScrollOffsetXForUnit(int unitFirstPage) const;
    int maxScrollOffsetYForUnit(int unitFirstPage) const;
    // Whether the unit needs an in-page scroll on PgUp/PgDn: only a vertical
    // overflow larger than the block-overlap band counts as real overflow. A
    // unit within the band (or exactly fitted) advances to the next unit on the
    // first press (see design D3).
    bool unitRequiresVerticalScroll(int unitFirstPage) const;
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

    // Rotated on-screen size of a view unit (a single page, or a two-page
    // spread in a double-page state, after rotation). Shared by the fit
    // computations so both fit modes target the unit currently in view.
    struct UnitSizes {
        int width = 0;
        int height = 0;
    };

    void computeFitZoom();
    // Paged mode: after a navigation command, re-fit the active fit mode to the
    // NEW current unit instead of keeping the open/relayout-time anchor. No-op
    // on uniform documents (zoom unchanged) and in manual zoom. Continuous mode
    // deliberately keeps the document-wide anchored zoom.
    void refitAfterNavigation();
    // Rotated dimensions of the unit containing the current page. The fit
    // target for both fit modes in paged mode (see design D1/D2).
    UnitSizes currentUnitSizes() const;
    // Combined on-screen size of a double-page unit given the two member sizes
    // (in the caller's units) and whether the unit has a partner. A singleton
    // unit collapses to the single page's size. Shared by computeLayout,
    // maxRowWidth and the fit computations so they never disagree (D8).
    static UnitSizes unitBounds(int firstW, int firstH,
                                int lastW, int lastH, bool paired);
    // Doc-wide maximum row width under the current rotation/presentation: the
    // combined spread width for a double-page state, the widest single page
    // otherwise. Fit-to-width targets this so no unit overflows the viewport
    // horizontally even off a singleton cover or trailing odd page.
    int maxRowWidth() const;
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

// Whether next/prev at the document boundary should (and can) open a sibling
    // image. Only true for a standalone raster document found in a sibling set.
    bool openSibling(int delta);

private:
    static float clampScale(float s) { return std::max(0.25f, std::min(s, 8.0f)); }

    bool isRasterPath(const QString& path);
    void scanSiblings(const QString& path);
    void stopAnimation();

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

    // Standalone-image folder context (sibling browsing + in-place animation).
    QString m_openPath;
    ImageFolder m_folder;
    int m_folderIndex = -1;
    int m_animationEpoch = 0;

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