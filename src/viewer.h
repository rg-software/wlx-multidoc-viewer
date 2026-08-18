#ifndef VIEWER_H
#define VIEWER_H

#include "document.h"
#include "viewerstate.h"

#include <QFrame>
#include <QScrollArea>
#include <QLabel>
#include <QPixmap>
#include <memory>

class ViewerWidget : public QFrame {
    Q_OBJECT
public:
    explicit ViewerWidget(QWidget* parent = nullptr);
    ~ViewerWidget() override;

    bool loadDocument(const QString& path);
    void closeDocument();

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
    void onFitWidth();
    void onFitPage();
    void onGoToPage();
    void onInfo();
    void onModeToggle();

private:
    void updatePage();
    void updateCounter();
    void updateZoomForFit();
    float dpiScale() const;

    std::unique_ptr<DocumentEngine> m_engine;
    ViewerState m_state;

    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_pageLabel = nullptr;
    QLabel* m_counterLabel = nullptr;
};

#endif // VIEWER_H
