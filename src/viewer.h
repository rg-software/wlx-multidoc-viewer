#ifndef VIEWER_H
#define VIEWER_H

#include "viewercontroller.h"
#include "sidebar.h"
#include "toolbar.h"

#include <QFrame>
#include <QKeyEvent>
#include <QPoint>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <QHBoxLayout>
#include <memory>

class ToolbarQt;
class SidebarQt;
class ViewerCanvas;
class QMouseEvent;

class ViewerWidget : public QFrame {
    Q_OBJECT
public:
    explicit ViewerWidget(QWidget* parent = nullptr);
    ~ViewerWidget() override;

    bool loadDocument(const QString& path);
    void closeDocument();

    ViewerController* controller() { return m_controller.get(); }

    // Focus the toolbar search box (host Find command / Ctrl+F).
    void focusFind();

    // Search-match navigation (F3 next / Shift+F3 previous). Driven by the
    // host's ListSearchDialog FindNext contract (plugin.cpp) and by the F3
    // key handler; both route through the toolbar presenter so the keyboard
    // and the Find buttons never diverge.
    void nextMatch();
    void prevMatch();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onNextPage();
    void onPrevPage();
    void onPageDown();
    void onPageUp();
    void onFirstPage();
    void onLastPage();
    void onZoomIn();
    void onZoomOut();
    void onZoomOriginal();
    void onCycleFit();
    void onToggleMode();
    void onTogglePresentation();
    void onToggleFavorite();
    void onFocusFind();
    void onRotateCw();
    void onRotateCcw();
    void onExitRequested();
    void onEscapePressed();
    void copySelection();
    void onVerticalScrollChanged(int value);

private:
    void onControllerChanged();
    void onFavoritesChanged();
    void syncAnimationTimer();
    void onAnimationTick();
    void resizeCanvas();
    void onSidebarToggle();
    void refreshChrome();
    int scrollYValue() const;
    QPointF widgetToCanvas(const QPoint& pos) const;
    QPoint eventPosForCanvas(const QMouseEvent* event, const QObject* obj) const;
    int pageAtCanvas(const QPointF& canvasPt) const;
    bool startSelection(const QPoint& pos);
    void extendSelection(const QPoint& pos);
    void endSelectionGesture();
    void clearSelectionUi();
    void refreshHoverCursor();
    bool onControlKey(QKeyEvent* event);
    void pageJumpContinuous(int delta);
    void stepVertical(int deltaPx);
    void stepVerticalTo(int y);
    void paintSearch(QPainter& p, const QRect& vis);

    std::unique_ptr<ViewerController> m_controller;

    ToolbarQt* m_toolbar = nullptr;
    SidebarQt* m_sidebar = nullptr;
    toolbar::ToolbarPresenter m_toolbarPresenter;
    SidebarPresenter m_sidebarPresenter;
    bool m_sidebarVisible = false;


    QHBoxLayout* m_midLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    ViewerCanvas* m_canvas = nullptr;

    // Single-shot timer that drives in-place GIF frame playback (design.md -
    // D6/D7); re-armed with the current frame's delay on each tick.
    QTimer* m_animTimer = nullptr;

    bool m_dragging = false;
    QPointF m_lastMousePos;
    bool m_suppressScrollTracking = false;
    // Fractional wheel-delta accumulator for Ctrl+wheel zoom so trackpads and
    // high-resolution wheels (|angleDelta| < a full step per event) zoom once
    // per notch instead of per event.
    int m_wheelZoomRemainder = 0;
    bool m_selecting = false;
    // Last local pointer position over the canvas. Wayland/compositor hosting
    // cannot be trusted for QCursor::pos()/mapFromGlobal (the viewer is
    // embedded in another toolkit's surface), so the hover refresh uses this
    // event-derived position instead.
    QPoint m_lastHoverPos;
    bool m_hoverPosValid = false;
    // Ctrl is tracked from the raw key events (KeyPress/KeyRelease of
    // Qt::Key_Control) instead of QGuiApplication::keyboardModifiers(): the
    // global query can lag or miss state on Wayland/embedded hosts, and the
    // release event may still carry ControlModifier. The hand cursor must come
    // and go deterministically on the modifier transitions themselves.
    bool m_ctrlDown = false;
};

// Pure-paint canvas that draws each page from the controller's render cache at
// its layout rect, plus text-selection and search-match overlays. Continuous
// mode makes this widget the full virtual canvas; paged mode sizes it to the
// current page.
class ViewerCanvas : public QWidget {
public:
    explicit ViewerCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        // Take click focus so key events (Ctrl+F, Ctrl+C, link-hover on the
        // Ctrl transition) reach the ViewerWidget's event filter. NoFocus
        // leaves the host's lister control in charge of the keyboard.
        setFocusPolicy(Qt::StrongFocus);
    }

    void setController(ViewerController* controller) { m_controller = controller; }
    void setContentSize(const QSize& size) { setFixedSize(size); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void paintSelection(QPainter& p, const QRect& vis) const;
    void paintSearchOverlay(QPainter& p, const QRect& vis) const;

    ViewerController* m_controller = nullptr;
};

#endif // VIEWER_H