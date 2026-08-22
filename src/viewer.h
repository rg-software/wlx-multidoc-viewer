#ifndef VIEWER_H
#define VIEWER_H

#include "viewercontroller.h"

#include <QFrame>
#include <QScrollArea>
#include <QLabel>
#include <QPixmap>
#include <QHBoxLayout>
#include <memory>

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

private:
    void onControllerChanged();
    void updateInfoPanel();
    void captureScrollForContinuousJump();
    void restoreScrollAfterContinuousJump();

    std::unique_ptr<ViewerController> m_controller;

    QHBoxLayout* m_infoLayout = nullptr;
    QLabel* m_pageIndicator = nullptr;
    QLabel* m_continuousIndicator = nullptr;
    QLabel* m_fitIndicator = nullptr;
    QFrame* m_infoBar = nullptr;

    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_pageLabel = nullptr;

    int m_savedScrollY = 0;
    bool m_pendingScrollRestore = false;
};

#endif // VIEWER_H
