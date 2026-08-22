#include "viewer.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QShortcut>
#include <QTimer>

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
        // Canvas is at least the viewport; center the page only if it fits.
        const int x = (width() - img.width()) / 2;
        const int y = (height() - img.height()) / 2;
        p.drawImage(std::max(0, x), std::max(0, y), img);
        paintSelection(p, rect());
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
    paintSelection(p, vis);
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
    m_canvas->setController(m_controller.get());

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
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this, &ViewerWidget::onEscapePressed);
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), this), &QShortcut::activated, this, &ViewerWidget::copySelection);
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
        // Paged mode: the canvas is at least the viewport; if the current page
        // overflows, the canvas grows to the page so the scroll area offers
        // horizontal (and, for a tall page, vertical) panning while centered
        // when it fits.
        const QSize vp = m_scrollArea->viewport()->size();
        const QRect pr = m_controller->pageRect(m_controller->currentPage());
        if (pr.isValid()) {
            QSize canvasSize = vp;
            if (pr.width() > vp.width())
                canvasSize.setWidth(pr.width());
            if (pr.height() > vp.height())
                canvasSize.setHeight(pr.height());
            m_canvas->setContentSize(canvasSize);
        } else {
            m_canvas->setContentSize(vp);
        }
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
    const int page = m_controller->currentPage();
    m_suppressScrollTracking = true;
    m_controller->toggleMode();
    if (m_controller->isPagedMode()) {
        m_scrollArea->verticalScrollBar()->setValue(0);
        m_suppressScrollTracking = false;
        resizeCanvas();
        return;
    }
    resizeCanvas();
    // The widget resize that sets the scrollbar's maximum may be deferred, so
    // set the value on the next event-loop spin (range is correct by then).
    // Suppression stays on so a transient valueChanged(0) during the resize
    // cannot reset the tracked page to 1.
    QTimer::singleShot(0, this, [this, page, target = m_controller->scrollOffsetForPage(page)]() {
        m_scrollArea->verticalScrollBar()->setValue(target);
        m_suppressScrollTracking = false;
    });
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

void ViewerWidget::copySelection() {
    if (!m_controller || !m_controller->hasSelection())
        return;
    const QString text = m_controller->selectedText();
    if (!text.isEmpty())
        QGuiApplication::clipboard()->setText(text);
}

void ViewerWidget::onEscapePressed() {
    if (m_controller && m_controller->hasSelection()) {
        clearSelectionUi();
        return; // Esc clears the selection; a second Esc exits the viewer.
    }
    onExitRequested();
}

void ViewerWidget::clearSelectionUi() {
    if (m_selecting)
        endSelectionGesture();
    if (m_controller)
        m_controller->clearSelection();
    if (m_canvas)
        m_canvas->update();
}

// Map a widget (canvas) position to content-canvas coordinates. Continuous
// mode: the canvas IS the content canvas (scrollbar offsets applied by
// QScrollArea). Paged mode: the canvas is the viewport and the page is
// centered; shift to the pageRect space the controller expects.
QPointF ViewerWidget::widgetToCanvas(const QPoint& pos) const {
    if (m_controller->isPagedMode()) {
        const QRect pr = m_controller->pageRect(m_controller->currentPage());
        const QSize vp = m_scrollArea->viewport()->size();
        const int dx = (vp.width() - pr.width()) / 2;
        const int dy = (vp.height() - pr.height()) / 2;
        return QPointF(pos.x() - dx + pr.x(), pos.y() - dy + pr.y());
    }
    return QPointF(pos.x(), pos.y());
}

int ViewerWidget::pageAtCanvas(const QPointF& canvasPt) const {
    if (m_controller->isPagedMode())
        return m_controller->currentPage();
    for (int page = 1; page <= m_controller->pageCount(); ++page) {
        if (m_controller->pageRect(page).contains(canvasPt.toPoint()))
            return page;
    }
    return -1;
}

bool ViewerWidget::startSelection(const QPoint& pos) {
    if (!m_controller)
        return false;
    const QPointF canvasPt = widgetToCanvas(pos);
    const int page = pageAtCanvas(canvasPt);
    if (page < 1 || !m_controller->pageHasText(page))
        return false;
    const int word = m_controller->wordAtCanvas(page, canvasPt);
    if (word < 0)
        return false;
    m_controller->beginSelection(page, word);
    m_selecting = true;
    setCursor(Qt::IBeamCursor);
    return true;
}

void ViewerWidget::extendSelection(const QPoint& pos) {
    if (!m_selecting)
        return;
    const QPointF canvasPt = widgetToCanvas(pos);
    int page = pageAtCanvas(canvasPt);
    if (page < 1)
        page = m_controller->currentPage();
    const int word = m_controller->wordAtCanvas(page, canvasPt);
    if (word < 0)
        return;
    m_controller->updateSelection(page, word);
    if (m_canvas)
        m_canvas->update();
}

void ViewerWidget::endSelectionGesture() {
    if (!m_selecting)
        return;
    m_selecting = false;
    m_controller->endSelection();
    setCursor(Qt::ArrowCursor);
}

void ViewerWidget::paintSelection(QPainter& p, const QRect& vis) {
    if (!m_controller || !m_controller->hasSelection())
        return;

    if (m_controller->isPagedMode()) {
        // Paged: the current page is centered on the viewport-sized canvas.
        const int page = m_controller->currentPage();
        const QRect pr = m_controller->pageRect(page);
        if (!pr.isValid())
            return;
        const QVector<QRectF> rects = m_controller->highlightRects(page);
        if (rects.isEmpty())
            return;
        const QSize vp = m_scrollArea->viewport()->size();
        const int dx = (vp.width() - pr.width()) / 2;
        const int dy = (vp.height() - pr.height()) / 2;
        p.setBrush(QColor(120, 140, 255, 90));
        p.setPen(Qt::NoPen);
        for (const QRectF& r : rects) {
            p.drawRect(r.translated(dx - pr.x(), dy - pr.y()));
        }
        return;
    }

    // Continuous: highlight rects are canvas-space; paint pages in the band.
    const int first = m_controller->firstPageAtScroll(vis.y());
    for (int page = first; page <= m_controller->pageCount(); ++page) {
        const QRect pr = m_controller->pageRect(page);
        if (pr.y() > vis.bottom())
            break;
        if (pr.bottom() < vis.y())
            continue;
        const QVector<QRectF> rects = m_controller->highlightRects(page);
        if (rects.isEmpty())
            continue;
        p.setBrush(QColor(120, 140, 255, 90));
        p.setPen(Qt::NoPen);
        for (const QRectF& r : rects) {
            p.drawRect(r);
        }
    }
}

// ESC exits the viewer by forwarding a synthetic Q keypress to DC's viewer
// panel (our parent widget), matching wlx-edge-viewer's ESC bridge: the host
// processes Q exactly like a physical press and runs its own cm_ExitViewer /
// lister-close path. The plugin never closes itself — that would race the
// host's close logic.
void ViewerWidget::onExitRequested() {
    QWidget* parent = parentWidget();
    if (!parent)
        return;
    QCoreApplication::postEvent(parent,
        new QKeyEvent(QEvent::KeyPress, Qt::Key_Q, Qt::NoModifier));
    QCoreApplication::postEvent(parent,
        new QKeyEvent(QEvent::KeyRelease, Qt::Key_Q, Qt::NoModifier));
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
    if (m_suppressScrollTracking)
        return;
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
        if (me->button() != Qt::LeftButton)
            break;
        const QPoint pos = me->position().toPoint();
        // Selection first: press on selectable text starts a selection.
        if (startSelection(pos))
            return true;
        // Otherwise the existing pan path, with the same overflow policy.
        if (m_controller->isPagedMode()) {
            const QRect pr = m_controller->pageRect(m_controller->currentPage());
            const QSize vp = m_scrollArea->viewport()->size();
            const bool overflows = pr.isValid() && (pr.width() > vp.width() || pr.height() > vp.height());
            if (!overflows)
                break;
        }
        m_dragging = true;
        m_lastMousePos = me->position().toPoint();
        setCursor(Qt::PointingHandCursor);
        return true;
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint pos = me->position().toPoint();
        if (m_selecting) {
            extendSelection(pos);
            return true;
        }
        if (!m_dragging) {
            // Hover: I-beam over selectable text.
            if (m_controller && m_controller->hasDocument()) {
                const QPointF canvasPt = widgetToCanvas(pos);
                const int page = pageAtCanvas(canvasPt);
                const bool overText = page >= 1 && m_controller->pageHasText(page) &&
                                      m_controller->wordAtCanvas(page, canvasPt) >= 0;
                setCursor(overText ? Qt::IBeamCursor : (m_dragging ? Qt::OpenHandCursor : Qt::ArrowCursor));
            }
            break;
        }
        const QPoint delta = m_lastMousePos - pos;
        m_lastMousePos = pos;
        QScrollBar* vBar = m_scrollArea->verticalScrollBar();
        QScrollBar* hBar = m_scrollArea->horizontalScrollBar();
        vBar->setValue(vBar->value() + delta.y());
        hBar->setValue(hBar->value() + delta.x());
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton)
            break;
        if (m_selecting) {
            endSelectionGesture();
            return true;
        }
        if (!m_dragging)
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