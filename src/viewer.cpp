#include "viewer.h"

#include <QVBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QScreen>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ViewerWidget::ViewerWidget(QWidget* parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_scrollArea);

    m_pageLabel = new QLabel(m_scrollArea);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidget(m_pageLabel);

    m_counterLabel = new QLabel(m_scrollArea);
    m_counterLabel->setAlignment(Qt::AlignBottom | Qt::AlignRight);
    m_counterLabel->setStyleSheet("QLabel { background: rgba(0,0,0,128); color: white; padding: 2px 6px; font-size: 11px; }");
    m_counterLabel->setAutoFillBackground(false);

    connect(new QShortcut(QKeySequence(Qt::Key_Right), this), &QShortcut::activated, this, &ViewerWidget::onNextPage);
    connect(new QShortcut(QKeySequence(Qt::Key_Left), this), &QShortcut::activated, this, &ViewerWidget::onPrevPage);
    connect(new QShortcut(QKeySequence(Qt::Key_Home), this), &QShortcut::activated, this, &ViewerWidget::onFirstPage);
    connect(new QShortcut(QKeySequence(Qt::Key_End), this), &QShortcut::activated, this, &ViewerWidget::onLastPage);
    connect(new QShortcut(QKeySequence(Qt::Key_PageDown), this), &QShortcut::activated, this, &ViewerWidget::onNextPage);
    connect(new QShortcut(QKeySequence(Qt::Key_PageUp), this), &QShortcut::activated, this, &ViewerWidget::onPrevPage);
    connect(new QShortcut(QKeySequence(Qt::Key_V), this), &QShortcut::activated, this, &ViewerWidget::onModeToggle);
    connect(new QShortcut(QKeySequence(Qt::Key_Plus), this), &QShortcut::activated, this, &ViewerWidget::onZoomIn);
    connect(new QShortcut(QKeySequence(Qt::Key_Equal), this), &QShortcut::activated, this, &ViewerWidget::onZoomIn);
    connect(new QShortcut(QKeySequence(Qt::Key_Minus), this), &QShortcut::activated, this, &ViewerWidget::onZoomOut);
    connect(new QShortcut(QKeySequence(Qt::Key_0), this), &QShortcut::activated, this, &ViewerWidget::onZoomOriginal);
    connect(new QShortcut(QKeySequence(Qt::Key_W), this), &QShortcut::activated, this, &ViewerWidget::onFitWidth);
    connect(new QShortcut(QKeySequence(Qt::Key_P), this), &QShortcut::activated, this, &ViewerWidget::onFitPage);
}

ViewerWidget::~ViewerWidget() {
    closeDocument();
}

bool ViewerWidget::loadDocument(const QString& path) {
    closeDocument();

    m_engine = createEngine(path);
    if (!m_engine || !m_engine->open(path))
        return false;

    m_state.setPageCount(m_engine->pageCount());
    m_state.resetPage();
    m_state.resetZoom();

    updateZoomForFit();
    updatePage();
    return true;
}

void ViewerWidget::closeDocument() {
    if (m_engine) {
        m_engine->close();
        m_engine.reset();
    }
    m_state = ViewerState();
    m_pageLabel->clear();
    m_counterLabel->clear();
}

float ViewerWidget::dpiScale() const {
    QScreen* s = screen();
    if (!s)
        return 1.0f;
    return s->logicalDotsPerInchX() / 96.0f;
}

void ViewerWidget::updatePage() {
    if (!m_engine || !m_engine->isOpen())
        return;

    QImage img = m_engine->renderPage(m_state.currentPage(), m_state.zoom(), dpiScale());
    if (!img.isNull()) {
        m_pageLabel->setPixmap(QPixmap::fromImage(img));
    } else {
        m_pageLabel->setText(tr("Failed to render page %1").arg(m_state.currentPage()));
    }
    updateCounter();
}

void ViewerWidget::updateCounter() {
    const char* mode = m_state.isPagedMode() ? "Page" : "Cont";
    m_counterLabel->setText(QString("%1/%2  [%3]").arg(m_state.currentPage()).arg(m_state.pageCount()).arg(mode));
}

void ViewerWidget::updateZoomForFit() {
    if (!m_engine || !m_engine->isOpen())
        return;

    if (!m_state.autoFit())
        return;

    PageInfo info = m_engine->pageDimensions(m_state.currentPage());
    if (info.width <= 0)
        return;

    int vw = m_scrollArea->viewport()->width();
    int vh = m_scrollArea->viewport()->height();
    float ds = dpiScale();

    if (m_state.fitToWidth()) {
        m_state.setZoom(m_state.fitToWidthZoom(info.width, vw) / ds);
    } else {
        m_state.setZoom(m_state.fitToPageZoom(info.width, info.height, vw, vh) / ds);
    }
}

void ViewerWidget::onNextPage() {
    if (m_state.nextPage()) {
        updateZoomForFit();
        updatePage();
    }
}

void ViewerWidget::onPrevPage() {
    if (m_state.prevPage()) {
        updateZoomForFit();
        updatePage();
    }
}

void ViewerWidget::onFirstPage() {
    if (m_state.firstPage()) {
        updateZoomForFit();
        updatePage();
    }
}

void ViewerWidget::onLastPage() {
    if (m_state.lastPage()) {
        updateZoomForFit();
        updatePage();
    }
}

void ViewerWidget::onZoomIn() {
    m_state.zoomIn();
    updatePage();
}

void ViewerWidget::onZoomOut() {
    m_state.zoomOut();
    updatePage();
}

void ViewerWidget::onZoomOriginal() {
    m_state.setManualZoom(1.0f);
    updatePage();
}

void ViewerWidget::onFitToggle() {
    if (m_state.autoFit()) {
        if (m_state.fitToWidth())
            m_state.setFitToPage();
        else
            m_state.setFitToWidth();
    } else {
        m_state.setFitToPage();
    }
    updateZoomForFit();
    updatePage();
}

void ViewerWidget::onFitWidth() {
    m_state.setFitToWidth();
    updateZoomForFit();
    updatePage();
}

void ViewerWidget::onFitPage() {
    m_state.setFitToPage();
    updateZoomForFit();
    updatePage();
}

void ViewerWidget::onModeToggle() {
    m_state.setPagedMode(!m_state.isPagedMode());
    updateCounter();
}

void ViewerWidget::onGoToPage() {
    if (!m_engine || !m_engine->isOpen())
        return;

    bool ok;
    int page = QInputDialog::getInt(this, tr("Go to page"),
                                     tr("Page number:"), m_state.currentPage(),
                                     1, m_state.pageCount(), 1, &ok);
    if (ok && m_state.goToPage(page)) {
        updateZoomForFit();
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
    if (m_engine && m_engine->isOpen()) {
        updateZoomForFit();
        updatePage();
    }
}
