#ifndef VIEWERCONTROLLER_H
#define VIEWERCONTROLLER_H

#include "document.h"
#include "viewer_settings.h"
#include "viewerstate.h"

#include <QImage>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>
#include <algorithm>
#include <functional>
#include <memory>

class ViewerController {
public:
    enum class FitMode { Manual, FitToPage, FitToWidth };

    // Shared height of the Win32 info panel / Qt info bar (single source:
    // viewer_settings.h).
    static constexpr int kInfoPanelHeight = viewer_settings::kInfoPanelHeight;

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
    void setDpiScale(float scale) { m_dpiScale = std::max(0.25f, std::min(scale, 8.0f)); }
    float dpiScale() const { return m_dpiScale; }

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
    int scrollOffsetForPage(int page) const;
    void trackCurrentPage(int page);
    void trimRenderCache(int scrollY);
    int layoutEpoch() const { return m_layoutEpoch; }

    // Relays a fit/zoom mode change that the caller already applied (fit mode,
    // viewport size, rotation angle) into a fresh layout, anchoring scrollY.
    int relayout(int scrollY);

    // Accessors
    int currentPage() const { return m_state.currentPage(); }
    int pageCount() const { return m_state.pageCount(); }
    float zoom() const { return m_state.zoom(); }
    FitMode fitMode() const { return m_fitMode; }
    int rotation() const { return m_rotation; }
    bool hasDocument() const { return m_engine && m_engine->isOpen(); }

    void setStateChangedCallback(StateChangedCallback cb) { m_onChanged = std::move(cb); }

private:
    void computeFitZoom();
    void computeLayout();
    int clampScroll(int scrollY) const;
    void notifyChanged();

    std::unique_ptr<DocumentEngine> m_engine;
    ViewerState m_state;
    FitMode m_fitMode = FitMode::FitToPage;
    int m_rotation = 0;
    QSize m_viewportSize;
    float m_dpiScale = 1.0f;
    StateChangedCallback m_onChanged;

    // Per-page layout in device pixels. m_pageRects[page-1].top() is the
    // cumulative top of the page in the (unbounded) content canvas; the canvas
    // width is max(page widths, viewport width) and pages are centered in it.
    QVector<QRect> m_pageRects;
    QSize m_contentSize;
    int m_layoutEpoch = 0;

    // Per-page render cache. m_pageCache[page-1] is null when not rendered;
    // m_cacheRecency is most-recent-first for eviction beyond the cache window.
    QVector<QImage> m_pageCache;
    QVector<int> m_cacheRecency;
};

#endif // VIEWERCONTROLLER_H