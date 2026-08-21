#ifndef VIEWERCONTROLLER_H
#define VIEWERCONTROLLER_H

#include "document.h"
#include "viewerstate.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <functional>
#include <memory>

// SumatraPDF reference patterns (read-only; not copied)
// ---------------------------------------------------
// Continuous-mode page strip layout: SumatraPDF renders all currently-visible
// pages into a single tall bitmap and lets the scrollbar walk through it. We
// do the same in ViewerController::renderVisiblePages() with a cap of 3x
// viewport height to bound memory on long documents.
//
// Wheel-event handling: SumatraPDF advances by 1/8 of the viewport per wheel
// notch in continuous mode, but jumps one page per notch in paged mode. We
// follow the same mode-dependent rule (see ViewerController::onWheel).
//
// Paged<->continuous transition: SumatraPDF anchors the scroll position so
// the page that was at the top of the viewport stays at the top. We do the
// same in ViewerController::toggleMode().
//
// Engine abstraction: SumatraPDF's EngineBase shows the value of one render
// method that takes (page, transform, target_size). Our DocumentEngine stays
// as (page, zoom, dpiScale, rotation) per the existing project layout.
// ---------------------------------------------------

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

    // Zoom / fit
    void zoomIn();
    void zoomOut();
    void setManualZoom(float zoom);
    void cycleFitMode();

    // Display mode
    void toggleMode();
    bool isPagedMode() const { return m_state.isPagedMode(); }

    // Rotation
    void rotateCw();
    void rotateCcw();

    // Viewport
    void setViewportSize(const QSize& size);
    int pageAreaHeight() const;
    int pageAreaWidth() const;

    // Render
    QImage renderVisiblePages();

    // Continuous-mode helpers
    int pageAtScrollOffset(int scrollY) const;
    int pageStride() const;
    void trackCurrentPage(int page);

    // Wheel
    void onWheel(int delta);

    // Accessors
    int currentPage() const { return m_state.currentPage(); }
    int pageCount() const { return m_state.pageCount(); }
    float zoom() const { return m_state.zoom(); }
    FitMode fitMode() const { return m_fitMode; }
    int rotation() const { return m_rotation; }
    bool hasDocument() const { return m_engine && m_engine->isOpen(); }

    void setStateChangedCallback(StateChangedCallback cb) { m_onChanged = std::move(cb); }

private:
    void recomputeFitZoom();
    void notifyChanged();

    std::unique_ptr<DocumentEngine> m_engine;
    ViewerState m_state;
    FitMode m_fitMode = FitMode::FitToPage;
    int m_rotation = 0;
    QSize m_viewportSize;
    StateChangedCallback m_onChanged;
};

#endif // VIEWERCONTROLLER_H
