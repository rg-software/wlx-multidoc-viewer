#include "viewer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>

ViewerWidget::ViewerWidget(QWidget* parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    layout->addWidget(m_toolbar);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_scrollArea);

    m_pageLabel = new QLabel(m_scrollArea);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidget(m_pageLabel);

    // Navigation actions
    auto* actFirst = m_toolbar->addAction(QIcon::fromTheme("go-first"), tr("First page"));
    connect(actFirst, &QAction::triggered, this, &ViewerWidget::onFirstPage);

    auto* actPrev = m_toolbar->addAction(QIcon::fromTheme("go-previous"), tr("Previous page"));
    connect(actPrev, &QAction::triggered, this, &ViewerWidget::onPrevPage);

    auto* actNext = m_toolbar->addAction(QIcon::fromTheme("go-next"), tr("Next page"));
    connect(actNext, &QAction::triggered, this, &ViewerWidget::onNextPage);

    auto* actLast = m_toolbar->addAction(QIcon::fromTheme("go-last"), tr("Last page"));
    connect(actLast, &QAction::triggered, this, &ViewerWidget::onLastPage);

    m_toolbar->addSeparator();

    m_counterLabel = new QLabel(m_toolbar);
    m_counterLabel->setMinimumWidth(60);
    m_counterLabel->setAlignment(Qt::AlignCenter);
    m_toolbar->addWidget(m_counterLabel);

    auto* actGoTo = m_toolbar->addAction(QIcon::fromTheme("go-jump"), tr("Go to..."));
    connect(actGoTo, &QAction::triggered, this, &ViewerWidget::onGoToPage);

    m_toolbar->addSeparator();

    // Zoom actions
    auto* actZoomIn = m_toolbar->addAction(QIcon::fromTheme("zoom-in"), tr("Zoom In"));
    connect(actZoomIn, &QAction::triggered, this, &ViewerWidget::onZoomIn);

    auto* actZoomOut = m_toolbar->addAction(QIcon::fromTheme("zoom-out"), tr("Zoom Out"));
    connect(actZoomOut, &QAction::triggered, this, &ViewerWidget::onZoomOut);

    auto* actZoomOrig = m_toolbar->addAction(QIcon::fromTheme("zoom-original"), tr("Original Size"));
    connect(actZoomOrig, &QAction::triggered, this, &ViewerWidget::onZoomOriginal);

    m_toolbar->addSeparator();

    auto* actFit = m_toolbar->addAction(QIcon::fromTheme("zoom-fit-best"), tr("Fit"));
    actFit->setCheckable(true);
    actFit->setChecked(true);
    connect(actFit, &QAction::triggered, this, &ViewerWidget::onFitToggle);

    m_toolbar->addSeparator();

    auto* actInfo = m_toolbar->addAction(QIcon::fromTheme("dialog-information"), tr("Info"));
    connect(actInfo, &QAction::triggered, this, &ViewerWidget::onInfo);

    // Keyboard shortcuts
    QShortcut* scNext = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(scNext, &QShortcut::activated, this, &ViewerWidget::onNextPage);

    QShortcut* scPrev = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(scPrev, &QShortcut::activated, this, &ViewerWidget::onPrevPage);

    QShortcut* scFirst = new QShortcut(QKeySequence(Qt::Key_Home), this);
    connect(scFirst, &QShortcut::activated, this, &ViewerWidget::onFirstPage);

    QShortcut* scLast = new QShortcut(QKeySequence(Qt::Key_End), this);
    connect(scLast, &QShortcut::activated, this, &ViewerWidget::onLastPage);

    QShortcut* scZoomIn = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
    connect(scZoomIn, &QShortcut::activated, this, &ViewerWidget::onZoomIn);

    QShortcut* scZoomOut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
    connect(scZoomOut, &QShortcut::activated, this, &ViewerWidget::onZoomOut);

    QShortcut* scFit = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_M), this);
    connect(scFit, &QShortcut::activated, this, &ViewerWidget::onFitToggle);

    QShortcut* scGoTo = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
    connect(scGoTo, &QShortcut::activated, this, &ViewerWidget::onGoToPage);

    QShortcut* scPgDown = new QShortcut(QKeySequence(Qt::Key_PageDown), this);
    connect(scPgDown, &QShortcut::activated, this, &ViewerWidget::onNextPage);

    QShortcut* scPgUp = new QShortcut(QKeySequence(Qt::Key_PageUp), this);
    connect(scPgUp, &QShortcut::activated, this, &ViewerWidget::onPrevPage);
}

ViewerWidget::~ViewerWidget() {
    closeDocument();
}

bool ViewerWidget::loadDocument(const QString& path) {
    closeDocument();

    m_engine = createEngine(path);
    if (!m_engine || !m_engine->open(path))
        return false;

    m_currentPage = 1;
    m_zoom = 1.0f;
    m_fitToWidth = true;

    updateZoomForFit();
    updatePage();
    return true;
}

void ViewerWidget::closeDocument() {
    if (m_engine) {
        m_engine->close();
        m_engine.reset();
    }
    m_pageLabel->clear();
    m_counterLabel->clear();
    m_currentPage = 1;
}

void ViewerWidget::updatePage() {
    if (!m_engine || !m_engine->isOpen())
        return;

    QImage img = m_engine->renderPage(m_currentPage, m_zoom);
    if (!img.isNull()) {
        m_pageLabel->setPixmap(QPixmap::fromImage(img));
    } else {
        m_pageLabel->setText(tr("Failed to render page %1").arg(m_currentPage));
    }
    updateCounter();
}

void ViewerWidget::updateCounter() {
    if (!m_engine || !m_engine->isOpen()) {
        m_counterLabel->clear();
        return;
    }
    m_counterLabel->setText(QString("%1/%2").arg(m_currentPage).arg(m_engine->pageCount()));
}

float ViewerWidget::fitToWidthZoom() const {
    if (!m_engine || !m_engine->isOpen())
        return 1.0f;

    PageInfo info = m_engine->pageDimensions(m_currentPage);
    if (info.width <= 0)
        return 1.0f;

    int viewportWidth = m_scrollArea->viewport()->width();
    return static_cast<float>(viewportWidth) / info.width;
}

float ViewerWidget::fitToPageZoom() const {
    if (!m_engine || !m_engine->isOpen())
        return 1.0f;

    PageInfo info = m_engine->pageDimensions(m_currentPage);
    if (info.width <= 0 || info.height <= 0)
        return 1.0f;

    float zx = static_cast<float>(m_scrollArea->viewport()->width()) / info.width;
    float zy = static_cast<float>(m_scrollArea->viewport()->height()) / info.height;
    return qMin(zx, zy);
}

void ViewerWidget::updateZoomForFit() {
    if (m_fitToWidth)
        m_zoom = fitToWidthZoom();
}

void ViewerWidget::onNextPage() {
    if (!m_engine || !m_engine->isOpen())
        return;
    if (m_currentPage < m_engine->pageCount()) {
        m_currentPage++;
        updatePage();
    }
}

void ViewerWidget::onPrevPage() {
    if (m_currentPage > 1) {
        m_currentPage--;
        updatePage();
    }
}

void ViewerWidget::onFirstPage() {
    if (m_currentPage != 1) {
        m_currentPage = 1;
        updatePage();
    }
}

void ViewerWidget::onLastPage() {
    if (!m_engine || !m_engine->isOpen())
        return;
    int last = m_engine->pageCount();
    if (m_currentPage != last) {
        m_currentPage = last;
        updatePage();
    }
}

void ViewerWidget::onZoomIn() {
    m_fitToWidth = false;
    m_zoom = qMin(m_zoom * 1.25f, 5.0f);
    updatePage();
}

void ViewerWidget::onZoomOut() {
    m_fitToWidth = false;
    m_zoom = qMax(m_zoom * 0.8f, 0.1f);
    updatePage();
}

void ViewerWidget::onZoomOriginal() {
    m_fitToWidth = false;
    m_zoom = 1.0f;
    updatePage();
}

void ViewerWidget::onFitToggle() {
    m_fitToWidth = !m_fitToWidth;
    updateZoomForFit();
    updatePage();
}

void ViewerWidget::onGoToPage() {
    if (!m_engine || !m_engine->isOpen())
        return;

    bool ok;
    int page = QInputDialog::getInt(this, tr("Go to page"),
                                     tr("Page number:"), m_currentPage,
                                     1, m_engine->pageCount(), 1, &ok);
    if (ok && page != m_currentPage) {
        m_currentPage = page;
        updatePage();
    }
}

void ViewerWidget::onInfo() {
    if (!m_engine || !m_engine->isOpen())
        return;

    QStringList fields;
    auto addField = [&](const QString& key, const QString& label) {
        QString val = m_engine->metadata(key);
        if (!val.isEmpty())
            fields << QString("<b>%1:</b> %2").arg(label, val.toHtmlEscaped());
    };

    addField("info:Title", tr("Title"));
    addField("info:Author", tr("Author"));
    addField("info:Subject", tr("Subject"));
    addField("info:Creator", tr("Creator"));
    addField("info:Producer", tr("Producer"));
    addField("info:CreationDate", tr("Creation Date"));
    addField("info:ModDate", tr("Modification Date"));

    if (fields.isEmpty()) {
        QMessageBox::information(this, tr("Document Info"),
                                  tr("No metadata available."));
    } else {
        QMessageBox::information(this, tr("Document Info"),
                                  fields.join("<br>"));
    }
}

void ViewerWidget::keyPressEvent(QKeyEvent* event) {
    QFrame::keyPressEvent(event);
}

void ViewerWidget::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    if (m_fitToWidth && m_engine && m_engine->isOpen()) {
        updateZoomForFit();
        updatePage();
    }
}
