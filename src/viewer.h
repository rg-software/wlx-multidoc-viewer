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
#include <QWheelEvent>
#include <QWidget>
#include <QHBoxLayout>
#include <memory>

class ToolbarQt;
class SidebarQt;
class ViewerCanvas;

class ViewerWidget : public QFrame {
    Q_OBJECT
public:
    explicit ViewerWidget(QWidget* parent = nullptr);
    ~ViewerWidget() override;

    bool loadDocument(const QString& path);
    void closeDocument();

    ViewerController* controller() { return m_controller.get(); }

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onNextPage();
    void onPrevPage();
    void onFirstPage();
    void onLastPage();
    void onZoomIn();
    void onZoomOut();
    void onZoomOriginal();
    void onCycleFit();
    void onToggleMode();
    void onRotateCw();
    void onRotateCcw();
    void onGoToPage();
    void onExitRequested();
    void onEscapePressed();
    void copySelection();
    void onVerticalScrollChanged(int value);

private:
    void onControllerChanged();
    void resizeCanvas();
    void onSidebarToggle();
    void refreshChrome();
    int scrollYValue() const;
    QPointF widgetToCanvas(const QPoint& pos) const;
    int pageAtCanvas(const QPointF& canvasPt) const;
    bool startSelection(const QPoint& pos);
    void extendSelection(const QPoint& pos);
    void endSelectionGesture();
    void clearSelectionUi();
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

    bool m_dragging = false;
    QPoint m_lastMousePos;
    bool m_suppressScrollTracking = false;
    bool m_selecting = false;
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