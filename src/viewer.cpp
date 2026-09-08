#include "viewer.h"
#include "viewer_settings.h"
#include "toolbar_qt.h"
#include "sidebar_qt.h"
#include "print_qt.h"
#include "ui_strings.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QShortcut>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

// Logical-px layout size of the current paged unit: a single page in the
// Single presentation, the whole spread in Double/DoubleWithCover. Returns a
// null QSize when the pages are not laid out yet. Shared by canvas sizing,
// painting, selection/search overlays and pan-overflow checks so they all
// treat the unit as one centered group.
QSize pagedUnitLayoutSize(const ViewerController* c) {
    if (!c || !c->hasDocument())
        return QSize();
    const int first = c->unitFirst(c->currentPage());
    const int last = c->unitLast(first);
    const QRect r1 = c->pageRect(first);
    if (!r1.isValid())
        return QSize();
    if (last == first)
        return r1.size();
    const QRect r2 = c->pageRect(last);
    if (!r2.isValid())
        return r1.size();
    return QSize(r2.right() - r1.left() + 1, qMax(r1.height(), r2.height()));
}

// Translation applied to any page-local canvas rect belonging to the current
// paged unit to place it on the canvas: pageRect.topLeft() cancels for every
// unit member, so the whole unit shares one offset.
QPointF pagedUnitCanvasOffset(const ViewerController* c, const QSize& canvasSize) {
    const QSize unit = pagedUnitLayoutSize(c);
    if (!unit.isValid() || unit.isEmpty())
        return QPointF();
    const QRect r1 = c->pageRect(c->unitFirst(c->currentPage()));
    if (!r1.isValid())
        return QPointF();
    return QPointF((canvasSize.width() - unit.width()) / 2 - r1.x(),
                   (canvasSize.height() - unit.height()) / 2 - r1.y());
}

} // namespace

void ViewerCanvas::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    const uint32_t bg = viewer_settings::kBackgroundColor;
    p.fillRect(event->rect(), QColor(static_cast<int>((bg >> 16) & 0xFF),
                                     static_cast<int>((bg >> 8) & 0xFF),
                                     static_cast<int>(bg & 0xFF)));

    if (!m_controller || !m_controller->hasDocument())
        return;

    // Bitmaps are rasterized at zoom*renderScale pixels while geometry lives
    // in layout units (logical px under Qt HiDPI). Drawing through an explicit
    // target rect maps one bitmap pixel onto one physical screen pixel without
    // touching the shared cached QImage (mutating its DPR would deep-copy).
    const float rs = m_controller->renderScale() > 0.0f ? m_controller->renderScale() : 1.0f;
    const float invRs = 1.0f / rs;
    if (rs > 1.0f)
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const bool paged = m_controller->isPagedMode();
    if (paged) {
        // Draw every page of the current unit (one page in Single, the spread
        // in Double/DoubleWithCover) at its shared placement. Canvas is at
        // least the viewport; the unit is centered only if it fits.
        const QPointF org = pagedUnitCanvasOffset(m_controller, size());
        const int first = m_controller->unitFirst(m_controller->currentPage());
        const int last = m_controller->unitLast(first);
        bool any = false;
        for (int page = first; page <= last; ++page) {
            QImage img = m_controller->renderPageCached(page);
            if (img.isNull())
                continue;
            any = true;
            const int lw = qRound(img.width() * invRs);
            const int lh = qRound(img.height() * invRs);
            const QRect r = m_controller->pageRect(page);
            const QPointF topLeft = QPointF(org.x() + r.x(), org.y() + r.y());
            p.drawImage(QRect(std::max(0, qRound(topLeft.x())),
                              std::max(0, qRound(topLeft.y())), lw, lh), img);
        }
        if (!any)
            return;
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
        p.drawImage(r, img);
    }
    m_controller->trimRenderCache(vis.y());
    paintSelection(p, vis);
    paintSearchOverlay(p, vis);
}

void ViewerCanvas::paintSelection(QPainter& p, const QRect& vis) const {
    if (!m_controller || !m_controller->hasSelection())
        return;

    if (m_controller->isPagedMode()) {
        const QPointF org = pagedUnitCanvasOffset(m_controller, size());
        p.setBrush(QColor(255, 240, 105, 105));
        p.setPen(Qt::NoPen);
        const int first = m_controller->unitFirst(m_controller->currentPage());
        const int last = m_controller->unitLast(first);
        for (int page = first; page <= last; ++page) {
            const QVector<QRectF> rects = m_controller->highlightRects(page);
            for (const QRectF& r : rects)
                p.drawRect(r.translated(org));
        }
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
    const int first = paged ? m_controller->unitFirst(m_controller->currentPage()) : m_controller->firstPageAtScroll(vis.y());
    const int last = paged ? m_controller->unitLast(first) : m_controller->pageCount();

    // One shared translation per unit: pageRect.topLeft() cancels for every
    // member, so page-local search rects land on the right bitmap.
    const QPointF org = paged ? pagedUnitCanvasOffset(m_controller, size()) : QPointF();

    for (int page = first; page <= last; ++page) {
        const QRect pr = m_controller->pageRect(page);
        if (!pr.isValid())
            continue;
        if (!paged && (pr.y() > vis.bottom() || pr.bottom() < vis.y()))
            continue;

        const QVector<QRectF> rects = m_controller->searchRectsOnPage(page);
        const QRectF active = m_controller->activeSearchRectOnPage(page);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 240, 105, 105));
        for (const QRectF& r : rects) {
            const QRectF rr = r.translated(org);
            p.drawRect(rr);
        }

        if (!active.isNull()) {
            const QRectF rr = active.translated(org);
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
    m_sidebar->setWidthChangedHandler([this](int logicalPx) {
        if (!m_controller)
            return;
        // Clamp the drag candidate to the shared bounds: min 80 logical px,
        // max half the page area (= clientLogicalW / 3 while the sidebar is
        // visible, never below the min so a tiny lister degrades gracefully).
        const int maxAllowed = (std::max)(viewer_settings::kSidebarMinWidth, width() / 3);
        const int clamped = (std::clamp)(logicalPx, viewer_settings::kSidebarMinWidth, maxAllowed);
        m_sidebar->setWidth(clamped);
        m_controller->setLeftChrome(clamped);
        m_scrollArea->verticalScrollBar()->setValue(m_controller->relayout(scrollYValue()));
        onControllerChanged();
    });

    m_scrollArea = new QScrollArea(mid);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_midLayout->addWidget(m_scrollArea, 1);

    layout->addWidget(mid, 1);

    m_canvas = new ViewerCanvas(m_scrollArea);
    m_scrollArea->setWidget(m_canvas);

    // The scroll-area viewport paints the page-area background around the
    // canvas whenever the canvas does not cover it (e.g. before the first
    // layout sizes the canvas, or in continuous mode when content is narrower
    // than the viewport). Give it the INI-fed BackgroundColor so the configured
    // color is visible from the very first paint instead of the default palette.
    {
        const uint32_t vbg = viewer_settings::kBackgroundColor;
        const QColor vbgColor(static_cast<int>((vbg >> 16) & 0xFF),
                              static_cast<int>((vbg >> 8) & 0xFF),
                              static_cast<int>(vbg & 0xFF));
        QPalette viewportPal = m_scrollArea->viewport()->palette();
        viewportPal.setColor(QPalette::Window, vbgColor);
        viewportPal.setColor(QPalette::Base, vbgColor);
        m_scrollArea->viewport()->setPalette(viewportPal);
        m_scrollArea->viewport()->setAutoFillBackground(true);
    }

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
    m_controller->setRenderScale(static_cast<float>(devicePixelRatioF()));

    // Single-shot timer driving in-place GIF frame playback (design.md - D6/D7);
    // parented to this widget so it dies with it.
    m_animTimer = new QTimer(this);
    m_animTimer->setSingleShot(true);
    connect(m_animTimer, &QTimer::timeout, this, &ViewerWidget::onAnimationTick);

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
    connect(new QShortcut(QKeySequence(Qt::Key_PageDown), this), &QShortcut::activated, this, &ViewerWidget::onPageDown);
    connect(new QShortcut(QKeySequence(Qt::Key_PageUp), this), &QShortcut::activated, this, &ViewerWidget::onPageUp);
    connect(new QShortcut(QKeySequence(Qt::Key_V), this), &QShortcut::activated, this, &ViewerWidget::onToggleMode);
    connect(new QShortcut(QKeySequence("Shift+V"), this), &QShortcut::activated, this, &ViewerWidget::onCycleFit);
    connect(new QShortcut(QKeySequence(Qt::Key_P), this), &QShortcut::activated, this, &ViewerWidget::onTogglePresentation);
    connect(new QShortcut(QKeySequence(Qt::Key_Plus), this), &QShortcut::activated, this, &ViewerWidget::onZoomIn);
    connect(new QShortcut(QKeySequence(Qt::Key_Equal), this), &QShortcut::activated, this, &ViewerWidget::onZoomIn);
    connect(new QShortcut(QKeySequence(Qt::Key_Minus), this), &QShortcut::activated, this, &ViewerWidget::onZoomOut);
    connect(new QShortcut(QKeySequence(Qt::Key_0), this), &QShortcut::activated, this, &ViewerWidget::onZoomOriginal);
    connect(new QShortcut(QKeySequence(Qt::Key_R), this), &QShortcut::activated, this, &ViewerWidget::onRotateCw);
    connect(new QShortcut(QKeySequence("Shift+R"), this), &QShortcut::activated, this, &ViewerWidget::onRotateCcw);
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
    m_controller->setRenderScale(static_cast<float>(devicePixelRatioF()));
    if (!m_controller->openDocument(path))
        return false;
    m_sidebarPresenter.reload();
    m_sidebarVisible = m_sidebarPresenter.hasOutline() && viewer_settings::kSidebarVisibleByDefault;
    m_sidebar->setVisible(m_sidebarVisible);
    refreshChrome();
    resizeCanvas();
    // reload() runs after openDocument() (which already fired refreshState),
    // so re-sync the toolbar now that sidebarAvailable()/hasOutline() are real.
    m_toolbarPresenter.refreshState();
    return true;
}

void ViewerWidget::closeDocument() {
    if (m_animTimer)
        m_animTimer->stop();
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
        const QSize unit = pagedUnitLayoutSize(m_controller.get());
        if (unit.isValid() && !unit.isEmpty()) {
            QSize canvasSize = vp;
            if (unit.width() > vp.width())
                canvasSize.setWidth(unit.width());
            if (unit.height() > vp.height())
                canvasSize.setHeight(unit.height());
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

// PgDn/PgUp scroll by one vertical block (one screenful with a small overlap).
// In paged mode a unit that still overflows scrolls within it; one that fits
// (or is within the overlap band of being so, or is already at its foot)
// advances to the next/prev page instead.
void ViewerWidget::onPageDown() {
    if (!m_controller || !m_controller->hasDocument())
        return;
    QScrollBar* vBar = m_scrollArea->verticalScrollBar();
    if (m_controller->isPagedMode()) {
        const int first = m_controller->unitFirst(m_controller->currentPage());
        const int maxY = m_controller->maxScrollOffsetYForUnit(first);
        if (m_controller->unitRequiresVerticalScroll(first) && scrollYValue() < maxY) {
            vBar->setValue(std::min(scrollYValue() + m_controller->pageBlockStep(), maxY));
            return;
        }
        if (m_controller->nextPage()) {
            m_suppressScrollTracking = true;
            vBar->setValue(0);
            m_suppressScrollTracking = false;
        }
        return;
    }
    vBar->setValue(std::min(scrollYValue() + m_controller->pageBlockStep(),
                            m_controller->maxScrollOffset()));
}

void ViewerWidget::onPageUp() {
    if (!m_controller || !m_controller->hasDocument())
        return;
    QScrollBar* vBar = m_scrollArea->verticalScrollBar();
    if (m_controller->isPagedMode()) {
        if (m_controller->unitRequiresVerticalScroll(
                m_controller->unitFirst(m_controller->currentPage())) && scrollYValue() > 0) {
            vBar->setValue(std::max(scrollYValue() - m_controller->pageBlockStep(), 0));
            return;
        }
        if (m_controller->prevPage()) {
            m_suppressScrollTracking = true;
            vBar->setValue(0);
            m_suppressScrollTracking = false;
        }
        return;
    }
    vBar->setValue(std::max(scrollYValue() - m_controller->pageBlockStep(), 0));
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

void ViewerWidget::onTogglePresentation() {
    if (!m_controller || !m_controller->hasDocument())
        return;
    // Mirror onToggleMode: keep the same unit in view while the spread shape
    // changes. Paged resets to a clean origin; continuous re-targets the scroll
    // to the unit's new top so the reader doesn't jump pages.
    const int page = m_controller->currentPage();
    m_suppressScrollTracking = true;
    m_controller->cyclePagePresentation();
    if (m_controller->isPagedMode()) {
        m_scrollArea->verticalScrollBar()->setValue(0);
        m_scrollArea->horizontalScrollBar()->setValue(0);
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
        // The unit is centered as one group, so the page-specific term cancels
        // and the mapping only needs the unit origin.
        const QSize vp = m_scrollArea->viewport()->size();
        const QSize unit = pagedUnitLayoutSize(m_controller.get());
        const QRect r1 = m_controller->pageRect(m_controller->unitFirst(m_controller->currentPage()));
        if (!unit.isEmpty() && r1.isValid()) {
            const int dx = (vp.width() - unit.width()) / 2;
            const int dy = (vp.height() - unit.height()) / 2;
            return QPointF(pos.x() - dx + r1.x(), pos.y() - dy + r1.y());
        }
        const QRect pr = m_controller->pageRect(m_controller->currentPage());
        const int dx = (vp.width() - pr.width()) / 2;
        const int dy = (vp.height() - pr.height()) / 2;
        return QPointF(pos.x() - dx + pr.x(), pos.y() - dy + pr.y());
    }
    return QPointF(pos.x(), pos.y());
}

int ViewerWidget::pageAtCanvas(const QPointF& canvasPt) const {
    if (m_controller->isPagedMode()) {
        // Any unit member can be the target of a selection/drag; the unit-first
        // fallback preserves the old single-page behavior for margins.
        const int first = m_controller->unitFirst(m_controller->currentPage());
        const int last = m_controller->unitLast(first);
        for (int page = first; page <= last; ++page) {
            if (m_controller->pageRect(page).contains(canvasPt.toPoint()))
                return page;
        }
        return m_controller->currentPage();
    }
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
    // Keep the toolbar Prev/Next enablement in sync with the new anchor: the
    // notify fired during goToPage refreshed against the pre-scroll position.
    m_toolbarPresenter.refreshState();
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
            const QSize unit = pagedUnitLayoutSize(m_controller.get());
            const QSize vp = m_scrollArea->viewport()->size();
            const bool overflows = unit.isValid() && !unit.isEmpty() &&
                                   (unit.width() > vp.width() || unit.height() > vp.height());
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
        m_controller->setRenderScale(static_cast<float>(devicePixelRatioF()));
        m_controller->setViewportSize(QSize(width(), height()));
        refreshChrome();
        m_scrollArea->verticalScrollBar()->setValue(m_controller->relayout(scrollYValue()));
    }
    resizeCanvas();
    syncAnimationTimer();
}

void ViewerWidget::syncAnimationTimer() {
    if (m_animTimer)
        m_animTimer->stop();
    if (!m_controller || !m_controller->isAnimating())
        return;
    const int delay = (std::max)(10, m_controller->animationDelayMs());
    if (delay > 0 && m_animTimer)
        m_animTimer->start(delay);
}

void ViewerWidget::onAnimationTick() {
    if (!m_controller || !m_controller->animationTick()) {
        if (m_animTimer)
            m_animTimer->stop();
        return;
    }
    // The controller advanced a frame and invalidated its page cache; repaint
    // the canvas so the new frame decodes (single page -- no relayout needed).
    if (m_canvas)
        m_canvas->update();
    if (m_animTimer)
        m_animTimer->start((std::max)(10, m_controller->animationDelayMs()));
}
