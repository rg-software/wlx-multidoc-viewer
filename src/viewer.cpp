#include "viewer.h"

#include <QInputDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QShortcut>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

void ViewerCanvas::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.fillRect(event->rect(), QColor(0x80, 0x80, 0x80));

    if (!m_controller || !m_controller->hasDocument())
        return;

    const bool paged = m_controller->isPagedMode();
    if (paged) {
        const int page = m_controller->currentPage();
        QImage img = m_controller->renderPageCached(page);
        if (img.isNull())
            return;
        const int x = (width() - img.width()) / 2;
        const int y = (height() - img.height()) / 2;
        p.drawImage(x, y, img);
        return;
    }

    // Continuous: the widget is the full canvas; the visible band is
    // event->rect() in canvas coordinates.
    const QRect vis = event->rect();
    if (m_controller->pageCount() <= 0 || vis.bottom() < 0 || vis.y() > m_controller->contentSize().height())
        return;

    const int firstVisible = m_controller->firstPageAtScroll(vis.y());
    for (int page = firstVisible; page <= m_controller->pageCount(); ++page) {
        const QRect r = m_controller->pageRect(page);
        if (r.y() > vis.bottom())
            break;
        if (r.bottom() < vis.y())
            continue;
        QImage img = m_controller->renderPageCached(page);
        if (img.isNull())
            continue;
        p.drawImage(r.x(), r.y(), img);
    }
    m_controller->trimRenderCache(vis.y());
}

ViewerWidget::ViewerWidget(QWidget* parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_infoBar = new QFrame(this);
    m_infoBar->setFixedHeight(ViewerController::kInfoPanelHeight);
    m_infoBar->setStyleSheet("QFrame { background: #202020; }");

    m_infoLayout = new QHBoxLayout(m_infoBar);
    m_infoLayout->setContentsMargins(8, 0, 8, 0);
    m_infoLayout->setSpacing(16);

    m_pageIndicator = new QLabel("- / -", m_infoBar);
    m_pageIndicator->setStyleSheet("QLabel { color: white; font-size: 11px; }");
    m_infoLayout->addWidget(m_pageIndicator);

    m_continuousIndicator = new QLabel("Continuous: OFF", m_infoBar);
    m_continuousIndicator->setStyleSheet("QLabel { color: white; font-size: 11px; }");
    m_infoLayout->addWidget(m_continuousIndicator);

    m_fitIndicator = new QLabel("Fit to page: OFF", m_infoBar);
    m_fitIndicator->setStyleSheet("QLabel { color: white; font-size: 11px; }");
    m_infoLayout->addWidget(m_fitIndicator);

    m_infoLayout->addStretch();

    layout->addWidget(m_infoBar);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_scrollArea);

    m_canvas = new ViewerCanvas(m_scrollArea);
    m_scrollArea->setWidget(m_canvas);

    m_canvas->installEventFilter(this);
    m_scrollArea->viewport()->installEventFilter(this);
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ViewerWidget::onVerticalScrollChanged);

    m_controller = std::make_unique<ViewerController>();
    m_controller->setStateChangedCallback([this]() { onControllerChanged(); });

    connect(new QShortcut(QKeySequence(Qt::Key_Right), this), &QShortcut::activated, this, &ViewerWidget::onNextPage);
    connect(new QShortcut(QKeySequence(Qt::Key_Left), this), &QShortcut::activated, this, &ViewerWidget::onPrevPage);
    connect(new QShortcut(QKeySequence(Qt::Key_Home), this), &QShortcut::activated, this, &ViewerWidget::onFirstPage);
    connect(new QShortcut(QKeySequence(Qt::Key_End), this), &QShortcut::activated, this, &ViewerWidget::onLastPage);
    connect(new QShortcut(QKeySequence(Qt::Key_PageDown), this), &QShortcut::activated, this, &ViewerWidget::onNextPage);
    connect(new QShortcut(QKeySequence(Qt::Key_PageUp), this), &QShortcut::activated, this, &ViewerWidget::onPrevPage);
    connect(new QShortcut(QKeySequence(Qt::Key_V), this), &QShortcut::activated, this, &ViewerWidget::onToggleMode);
    connect(new QShortcut(QKeySequence("Shift+V"), this), &QShortcut::activated, this, &ViewerWidget::onCycleFit);
    connect(new QShortcut(QKeySequence(Qt::Key_Plus), this), &QShortcut::activated, this, &ViewerWidget::onZoomIn);
    connect(new QShortcut(QKeySequence(Qt::Key_Equal), this), &QShortcut::activated, this, &ViewerWidget::onZoomIn);
    connect(new QShortcut(QKeySequence(Qt::Key_Minus), this), &QShortcut::activated, this, &ViewerWidget::onZoomOut);
    connect(new QShortcut(QKeySequence(Qt::Key_0), this), &QShortcut::activated, this, &ViewerWidget::onZoomOriginal);
    connect(new QShortcut(QKeySequence(Qt::Key_R), this), &QShortcut::activated, this, &ViewerWidget::onRotateCw);
    connect(new QShortcut(QKeySequence("Shift+R"), this), &QShortcut::activated, this, &ViewerWidget::onRotateCcw);
    connect(new QShortcut(QKeySequence(Qt::Key_G), this), &QShortcut::activated, this, &ViewerWidget::onGoToPage);
}

ViewerWidget::~ViewerWidget() {
    closeDocument();
}

bool ViewerWidget::loadDocument(const QString& path) {
    closeDocument();
    m_controller->setEngine(createEngine(path));
    m_controller->setDpiScale(static_cast<float>(devicePixelRatioF()));
    if (!m_controller->openDocument(path))
        return false;
    resizeCanvas();
    return true;
}

void ViewerWidget::closeDocument() {
    if (m_controller)
        m_controller->closeDocument();
    if (m_canvas)
        m_canvas->setContentSize(QSize(1, 1));
    updateInfoPanel();
}

void ViewerWidget::onControllerChanged() {
    resizeCanvas();
    updateInfoPanel();
}

void ViewerWidget::resizeCanvas() {
    if (!m_controller || !m_controller->hasDocument()) {
        m_canvas->setContentSize(QSize(1, 1));
        m_canvas->update();
        return;
    }
    if (m_controller->isPagedMode()) {
        // Paged mode: the canvas is the scroll viewport; no scrolling.
        const QSize vp = m_scrollArea->viewport()->size();
        m_canvas->setContentSize(vp);
    } else {
        m_canvas->setContentSize(m_controller->contentSize());
    }
    m_canvas->update();
}

int ViewerWidget::scrollYValue() const {
    return m_scrollArea->verticalScrollBar()->value();
}

void ViewerWidget::updateInfoPanel() {
    if (!m_controller || !m_controller->hasDocument()) {
        if (m_pageIndicator) m_pageIndicator->setText("- / -");
        if (m_continuousIndicator) m_continuousIndicator->setText("Continuous: -");
        if (m_fitIndicator) m_fitIndicator->setText("Fit: -");
        return;
    }
    m_pageIndicator->setText(QString("%1 / %2").arg(m_controller->currentPage()).arg(m_controller->pageCount()));
    m_continuousIndicator->setText(m_controller->isPagedMode() ? "Continuous: OFF" : "Continuous: ON");
    QString fitLabel;
    switch (m_controller->fitMode()) {
    case ViewerController::FitMode::FitToPage:  fitLabel = "Fit: Page"; break;
    case ViewerController::FitMode::FitToWidth: fitLabel = "Fit: Width"; break;
    case ViewerController::FitMode::Manual: {
        int pct = static_cast<int>(m_controller->zoom() * 100 + 0.5);
        fitLabel = QString("Zoom: %1%").arg(pct);
        break;
    }
    }
    m_fitIndicator->setText(fitLabel);
}

void ViewerWidget::onNextPage() {
    if (m_controller) m_controller->nextPage();
}

void ViewerWidget::onPrevPage() {
    if (m_controller) m_controller->prevPage();
}

void ViewerWidget::onFirstPage() {
    if (m_controller) m_controller->firstPage();
}

void ViewerWidget::onLastPage() {
    if (m_controller) m_controller->lastPage();
}

void ViewerWidget::onZoomIn() {
    if (!m_controller) return;
    m_scrollArea->verticalScrollBar()->setValue(m_controller->zoomIn(scrollYValue()));
}

void ViewerWidget::onZoomOut() {
    if (!m_controller) return;
    m_scrollArea->verticalScrollBar()->setValue(m_controller->zoomOut(scrollYValue()));
}

void ViewerWidget::onZoomOriginal() {
    if (!m_controller) return;
    m_scrollArea->verticalScrollBar()->setValue(m_controller->setManualZoom(1.0f, scrollYValue()));
}

void ViewerWidget::onCycleFit() {
    if (!m_controller) return;
    m_scrollArea->verticalScrollBar()->setValue(m_controller->cycleFitMode(scrollYValue()));
}

void ViewerWidget::onToggleMode() {
    if (!m_controller) return;
    m_controller->toggleMode();
    if (m_controller->isPagedMode()) {
        m_scrollArea->verticalScrollBar()->setValue(0);
    } else {
        m_scrollArea->verticalScrollBar()->setValue(m_controller->scrollOffsetForPage(m_controller->currentPage()));
    }
    resizeCanvas();
}

void ViewerWidget::onRotateCw() {
    if (!m_controller) return;
    m_scrollArea->verticalScrollBar()->setValue(m_controller->rotateCw(scrollYValue()));
}

void ViewerWidget::onRotateCcw() {
    if (!m_controller) return;
    m_scrollArea->verticalScrollBar()->setValue(m_controller->rotateCcw(scrollYValue()));
}

void ViewerWidget::onGoToPage() {
    if (!m_controller || !m_controller->hasDocument())
        return;

    bool ok;
    int page = QInputDialog::getInt(this, tr("Go to page"),
                                     tr("Page number:"), m_controller->currentPage(),
                                     1, m_controller->pageCount(), 1, &ok);
    if (ok) {
        m_controller->goToPage(page);
        if (!m_controller->isPagedMode()) {
            m_scrollArea->verticalScrollBar()->setValue(m_controller->scrollOffsetForPage(page));
        }
    }
}

void ViewerWidget::keyPressEvent(QKeyEvent* event) {
    QFrame::keyPressEvent(event);
}

void ViewerWidget::wheelEvent(QWheelEvent* event) {
    if (m_controller && m_controller->isPagedMode()) {
        if (event->angleDelta().y() < 0)
            m_controller->nextPage();
        else if (event->angleDelta().y() > 0)
            m_controller->prevPage();
        event->accept();
        return;
    }
    QFrame::wheelEvent(event);
}

void ViewerWidget::onVerticalScrollChanged(int value) {
    if (!m_controller || !m_controller->hasDocument() || m_controller->isPagedMode())
        return;
    const int page = m_controller->pageAtScrollOffset(value);
    if (page != m_controller->currentPage()) {
        m_controller->trackCurrentPage(page);
        updateInfoPanel();
    }
    m_controller->trimRenderCache(value);
}

bool ViewerWidget::eventFilter(QObject* obj, QEvent* event) {
    if (!m_controller || !m_controller->hasDocument()) {
        if (m_dragging) {
            m_dragging = false;
            unsetCursor();
        }
        return QFrame::eventFilter(obj, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton || m_controller->isPagedMode())
            break;
        m_dragging = true;
        m_lastMousePos = me->position().toPoint();
        setCursor(Qt::PointingHandCursor);
        return true;
    }
    case QEvent::MouseMove: {
        if (!m_dragging)
            break;
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint pos = me->position().toPoint();
        const QPoint delta = m_lastMousePos - pos;
        m_lastMousePos = pos;
        QScrollBar* vBar = m_scrollArea->verticalScrollBar();
        QScrollBar* hBar = m_scrollArea->horizontalScrollBar();
        vBar->setValue(vBar->value() + delta.y());
        hBar->setValue(hBar->value() + delta.x());
        return true;
    }
    case QEvent::MouseButtonRelease: {
        if (!m_dragging)
            break;
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton)
            break;
        m_dragging = false;
        unsetCursor();
        return true;
    }
    default:
        break;
    }

    return QFrame::eventFilter(obj, event);
}

bool ViewerWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(eventType)
    Q_UNUSED(result)

#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        auto* msg = static_cast<MSG*>(message);
        if (msg->message == WM_SIZE) {
            int w = LOWORD(msg->lParam);
            int h = HIWORD(msg->lParam);
            if (w > 0 && h > 0) {
                resize(w, h);
                return false;
            }
        }
    }
#else
    Q_UNUSED(message)
#endif

    return QFrame::nativeEvent(eventType, message, result);
}

void ViewerWidget::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    if (m_controller) {
        m_controller->setDpiScale(static_cast<float>(devicePixelRatioF()));
        m_controller->setViewportSize(QSize(width(), height()));
        const int y = scrollYValue();
        m_scrollArea->verticalScrollBar()->setValue(m_controller->relayout(y));
        resizeCanvas();
    }
}