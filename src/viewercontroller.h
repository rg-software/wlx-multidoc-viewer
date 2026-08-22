#ifndef VIEWERCONTROLLER_H
#define VIEWERCONTROLLER_H

#include "document.h"
#include "viewerstate.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <algorithm>
#include <functional>
#include <memory>

class ViewerController {
public:
    enum class FitMode { Manual, FitToPage, FitToWidth };

    // Shared height of the Win32 info panel / Qt info bar.
    static constexpr int kInfoPanelHeight = 22;

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
    void setDpiScale(float scale) { m_dpiScale = std::max(0.25f, std::min(scale, 8.0f)); }
    float dpiScale() const { return m_dpiScale; }

    // Render
    QImage renderVisiblePages(int scrollY = 0);

    // Continuous-mode helpers
    int pageAtScrollOffset(int scrollY) const;
    int pageStride() const;
    void trackCurrentPage(int page);

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
    float m_dpiScale = 1.0f;
    StateChangedCallback m_onChanged;
};

#endif // VIEWERCONTROLLER_H
