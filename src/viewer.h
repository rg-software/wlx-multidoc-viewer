#ifndef VIEWER_H
#define VIEWER_H

#include "document.h"

#include <QFrame>
#include <QScrollArea>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QPixmap>
#include <memory>

class ViewerWidget : public QFrame {
    Q_OBJECT
public:
    explicit ViewerWidget(QWidget* parent = nullptr);
    ~ViewerWidget() override;

    bool loadDocument(const QString& path);
    void closeDocument();
    DocumentEngine* engine() const { return m_engine.get(); }

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onNextPage();
    void onPrevPage();
    void onFirstPage();
    void onLastPage();
    void onZoomIn();
    void onZoomOut();
    void onZoomOriginal();
    void onFitToggle();
    void onGoToPage();
    void onInfo();

private:
    void updatePage();
    void updateCounter();
    void updateZoomForFit();
    float fitToWidthZoom() const;
    float fitToPageZoom() const;

    std::unique_ptr<DocumentEngine> m_engine;
    QToolBar* m_toolbar = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_pageLabel = nullptr;
    QLabel* m_counterLabel = nullptr;

    int m_currentPage = 1;
    float m_zoom = 1.0f;
    bool m_fitToWidth = true;
};

#endif // VIEWER_H
