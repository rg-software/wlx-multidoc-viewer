#include "viewer.h"
#include "toolbar_qt.h"
#include "sidebar_qt.h"
#include "print_qt.h"

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
    p.fillRect(event->rect(), QColor(0xE8, 0xE8, 0xE8));

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
        paintSearchOverlay(p, rect());
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
    paintSearchOverlay(p, vis);
}

void ViewerCanvas::paintSelection(QPainter& p, const QRect& vis) const {
    if (!m_controller || !m_controller->hasSelection())
        return;

    if (m_controller->isPagedMode()) {
        const int page = m_controller->currentPage();
        const QRect pr = m_controller->pageRect(page);
        if (!pr.isValid())
            return;
        const QVector<QRectF> rects = m_controller->highlightRects(page);
        if (rects.isEmpty())
            return;
        const QSize vp = size();
        const int dx = (vp.width() - pr.width()) / 2;
        const int dy = (vp.height() - pr.height()) / 2;
        p.setBrush(QColor(255, 240, 105, 105));
        p.setPen(Qt::NoPen);
        for (const QRectF& r : rects)
            p.drawRect(r.translated(dx - pr.x(), dy - pr.y()));
        return;
    }

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
        p.setBrush(QColor(255, 240, 105, 105));
        p.setPen(Qt::NoPen);
        for (const QRectF& r : rects)
            p.drawRect(r);
    }
}

void ViewerCanvas::paintSearchOverlay(QPainter& p, const QRect& vis) const {
    if (!m_controller || !m_controller->hasSearchHighlights())
        return;

    const bool paged = m_controller->isPagedMode();
    const int first = paged ? m_controller->currentPage() : m_controller->firstPageAtScroll(vis.y());
    const int last = paged ? m_controller->currentPage() : m_controller->pageCount();

    for (int page = first; page <= last; ++page) {
        const QRect pr = m_controller->pageRect(page);
        if (!pr.isValid())
            continue;
        if (!paged && (pr.y() > vis.bottom() || pr.bottom() < vis.y()))
            continue;

        const QVector<QRectF> rects = m_controller->searchRectsOnPage(page);
        const QRectF active = m_controller->activeSearchRectOnPage(page);

        QPointF origin(0, 0);
        if (paged) {
            const QSize vp = size();
            origin = QPointF((vp.width() - pr.width()) / 2 - pr.x(),
                             (vp.height() - pr.height()) / 2 - pr.y());
        }

p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 240, 105, 105));
        for (const QRectF& r : rects) {
            const QRectF rr = r.translated(origin);
            p.drawRect(rr);
        }

        if (!active.isNull()) {
            const QRectF rr = active.translated(origin);
            p.setBrush(QColor(0, 220, 220, 150));   // cyan active match
            p.setPen(QPen(QColor(0, 130, 130), 1));
            p.drawRect(rr);
        }
    }
}

// ESC exits the viewer by forwarding a synthetic Q keypress to DC's viewer
// panel (our parent widget), matching wlx-edge-viewer's ESC bridge.
void ViewerWidget::onExitRequested() {
    QWidget* parent = parentWidget();
    if (!parent)
        return;
    QCoreApplication::postEvent(parent,
        new QKeyEvent(QEvent::KeyPress, Qt::Key_Q, Qt::NoModifier));
    QCoreApplication::postEvent(parent,
        new QKeyEvent(QEvent::KeyRelease, Qt::Key_Q, Qt::NoModifier));
}

ViewerWidget::ViewerWidget(QWidget* parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_toolbar = new ToolbarQt(this);
    layout->addWidget(m_toolbar);

    auto* mid = new QWidget(this);
    m_midLayout = new QHBoxLayout(mid);
    m_midLayout->setContentsMargins(0, 0, 0, 0);
    m_midLayout->setSpacing(0);

    m_sidebar = new SidebarQt(mid);
    m_sidebar->setVisible(false);
    m_midLayout->addWidget(m_sidebar);

    m_scrollArea = new QScrollArea(mid);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_midLayout->addWidget(m_scrollArea, 1);

    layout->addWidget(mid, 1);

    m_canvas = new ViewerCanvas(m_scrollArea);
    m_scrollArea->setWidget(m_canvas);

    m_canvas->installEventFilter(this);
    m_scrollArea->viewport()->installEventFilter(this);
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ViewerWidget::onVerticalScrollChanged);

    m_controller = std::make_unique<ViewerController>();
    m_controller->setStateChangedCallback([this]() { onControllerChanged(); });
    m_controller->setUiMarshal([this](std::function<void()> task) {
        if (task)
            QMetaObject::invokeMethod(this, std::move(task), Qt::QueuedConnection);
    });
    m_canvas->setController(m_controller.get());
    m_controller->setDpiScale(static_cast<float>(devicePixelRatioF()));

    m_toolbarPresenter.attach(m_controller.get(), m_toolbar);
    m_toolbarPresenter.setScrollApplier([this](int scrollY) {
        m_controller->setScrollAnchor(scrollY);
        m_scrollArea->verticalScrollBar()->setValue(scrollY);
    });
    m_toolbarPresenter.printHandler = [this]() { printDocumentQt(this, m_controller.get()); };
    m_toolbarPresenter.sidebarToggleHandler = [this]() { onSidebarToggle(); };

    m_sidebarPresenter.attach(m_controller.get(), m_sidebar);
    m_sidebarPresenter.setScrollApplier([this](int scrollY) {
        m_controller->setScrollAnchor(scrollY);
        m_scrollArea->verticalScrollBar()->setValue(scrollY);
    });
    m_toolbarPresenter.sidebarAvailable = [this]() { return m_sidebarPresenter.hasOutline(); };
    m_toolbarPresenter.copyHandler = [this](const QString& text) {
        QGuiApplication::clipboard()->setText(text);
        m_canvas->update(); // drop the active selection highlight after copy
    };
    m_toolbarPresenter.sidebarVisible = [this]() { return m_sidebarVisible; };

    // Keyboard shortcuts (unchanged; focus neutrality is natural in Qt: line
    // edits consume their own keys).
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

    m_toolbarPresenter.refreshState();
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
    m_sidebarPresenter.reload();
    m_sidebarVisible = false;
    m_sidebar->setVisible(false);
    refreshChrome();
    resizeCanvas();
    // reload() runs after openDocument() (which already fired refreshState),
    // so re-sync the toolbar now that sidebarAvailable()/hasOutline() are real.
    m_toolbarPresenter.refreshState();
    return true;
}

void ViewerWidget::closeDocument() {
    if (m_controller)
        m_controller->closeDocument();
    m_sidebarPresenter.reload();
    if (m_canvas)
        m_canvas->setContentSize(QSize(1, 1));
    if (m_toolbarPresenter.backend())
        m_toolbarPresenter.refreshState();
}

void ViewerWidget::refreshChrome() {
    if (!m_controller)
        return;
    m_controller->setTopChrome(m_toolbar ? m_toolbar->height() : 0);
    m_controller->setBottomChrome(0);
    m_controller->setLeftChrome(m_sidebarVisible && m_sidebar ? m_sidebar->width() : 0);
}

void ViewerWidget::onSidebarToggle() {
    if (!m_sidebarPresenter.hasOutline() || !m_controller || !m_controller->hasDocument())
        return;
    m_sidebarVisible = !m_sidebarVisible;
    m_sidebar->setVisible(m_sidebarVisible);
    refreshChrome();
    const int y = scrollYValue();
    m_scrollArea->verticalScrollBar()->setValue(m_controller->relayout(y));
    m_toolbar->setChecked(toolbar::Control::SidebarToggle, m_sidebarVisible);
    resizeCanvas();
}

void ViewerWidget::onControllerChanged() {
    if (m_toolbarPresenter.backend())
        m_toolbarPresenter.refreshState();
    m_sidebarPresenter.onPageChanged(m_controller->currentPage());

    // A search that found matches should bring the first match into view.
    if (m_controller->hasPendingSearchJump()) {
        const int jump = m_controller->takeSearchJump();
        m_scrollArea->verticalScrollBar()->setValue(jump);
    }

    resizeCanvas();
}

void ViewerWidget::resizeCanvas() {
    if (!m_controller || !m_controller->hasDocument()) {
        m_canvas->setContentSize(QSize(1, 1));
        m_canvas->update();
        return;
    }
    if (m_controller->isPagedMode()) {
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
        if (!m_controller->isPagedMode())
            m_scrollArea->verticalScrollBar()->setValue(m_controller->scrollOffsetForPage(page));
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
    m_controller->clearSearch();
    if (m_controller->searchActive()) {
        m_canvas->update();
        return;
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
    const int word = m_controller->wordAtCanvas(page, canvasPt,
                                                viewer_settings::kSelectionHitTolerancePx);
    if (word < 0)
        return false; // empty area -> pan gesture proceeds
    const int ch = m_controller->charAtCanvas(page, word, canvasPt);
    m_controller->beginSelection(page, word, ch);
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
    const int word = m_controller->wordAtCanvas(page, canvasPt); // nearest while dragging
    if (word < 0)
        return;
    const int ch = m_controller->charAtCanvas(page, word, canvasPt);
    m_controller->updateSelection(page, word, ch);
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
    m_controller->setScrollAnchor(value);
    const int page = m_controller->pageAtScrollOffset(value);
    if (page != m_controller->currentPage()) {
        m_controller->trackCurrentPage(page);
        m_sidebarPresenter.onPageChanged(page);
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
        if (startSelection(pos))
            return true;
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
            if (m_controller && m_controller->hasDocument()) {
                const QPointF canvasPt = widgetToCanvas(pos);
                const int page = pageAtCanvas(canvasPt);
                const bool overText = page >= 1 && m_controller->pageHasText(page) &&
                                      m_controller->wordAtCanvas(page, canvasPt,
                                          viewer_settings::kSelectionHitTolerancePx) >= 0;
                setCursor(overText ? Qt::IBeamCursor : Qt::ArrowCursor);
            }
            break;
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
        refreshChrome();
        m_scrollArea->verticalScrollBar()->setValue(m_controller->relayout(scrollYValue()));
        resizeCanvas();
    }
}
