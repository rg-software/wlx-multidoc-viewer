#ifndef VIEWER_H
#define VIEWER_H

#include "viewercontroller.h"

#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QPoint>
#include <QResizeEvent>
#include <QScrollArea>
#include <QWheelEvent>
#include <QWidget>
#include <QHBoxLayout>
#include <memory>

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
    void onVerticalScrollChanged(int value);

private:
    void onControllerChanged();
    void updateInfoPanel();
    void resizeCanvas();
    int scrollYValue() const;

    std::unique_ptr<ViewerController> m_controller;

    QHBoxLayout* m_infoLayout = nullptr;
    QLabel* m_pageIndicator = nullptr;
    QLabel* m_continuousIndicator = nullptr;
    QLabel* m_fitIndicator = nullptr;
    QFrame* m_infoBar = nullptr;

    QScrollArea* m_scrollArea = nullptr;
    ViewerCanvas* m_canvas = nullptr;

    bool m_dragging = false;
    QPoint m_lastMousePos;
};

// Pure-paint canvas that draws each page from the controller's render cache at
// its layout rect. Continuous mode makes this widget the full virtual canvas;
// paged mode sizes it to the current page.
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
    ViewerController* m_controller = nullptr;
};

#endif // VIEWER_H